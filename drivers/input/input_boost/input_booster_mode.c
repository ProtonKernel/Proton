#define ITAG " [Input Booster] "
#include <linux/input/input_booster.h>

#include <linux/random.h>
#include <linux/spinlock.h>
#include <linux/syscalls.h>
#include <linux/module.h>
#include <linux/init.h>

#if IS_ENABLED(CONFIG_SEC_INPUT_BOOSTER_QC) || \
	IS_ENABLED(CONFIG_SEC_INPUT_BOOSTER_SLSI) || \
	IS_ENABLED(CONFIG_SEC_INPUT_BOOSTER_MTK)
spinlock_t write_ib_lock;
spinlock_t write_qos_lock;
struct mutex trigger_ib_lock;
struct mutex mem_lock;
struct mutex rel_ib_lock;
struct mutex sip_rel_lock;
struct workqueue_struct *ib_handle_highwq;

int total_ib_cnt;
int ib_init_succeed;
int level_value = IB_MAX;

unsigned int debug_flag;
unsigned int enable_event_booster = INIT_ZERO;
unsigned int u_ib_mode;

// Input Booster Init Variables
int *release_val;
int *cpu_cluster_policy;
int *allowed_resources;

int max_resource_count;
int max_cluster_count;
int allowed_res_count;

static int allowed_mask;
static struct t_ib_boost_mode* ib_boost_modes;
static unsigned int ib_mode_mask;
static int num_of_mode;

struct t_ib_device_tree* ib_device_trees;
struct t_ib_trigger* ib_trigger;

struct list_head* ib_list;
struct list_head* qos_list;

// @evdev_mt_slot : save the number of inputed touch slot.
int evdev_mt_slot;
// @evdev_mt_event[] : save count of each boooter's events.
int evdev_mt_event[MAX_DEVICE_TYPE_NUM];
int trigger_cnt;
int send_ev_enable;

struct t_ib_info* find_release_ib(int dev_type, int key_id);
struct t_ib_info* create_ib_instance(struct t_ib_trigger* p_IbTrigger, int uniqId);
bool is_validate_uniqid(unsigned int uniq_id);
struct t_ib_target* find_update_target(int uniq_id, int res_id);
long get_qos_value(int res_id);
void remove_ib_instance(struct t_ib_info* ib);

void trigger_input_booster(struct work_struct *work)
{
	unsigned int uniq_id = 0;
	int res_type = -1;

	struct t_ib_info* ib;
	struct t_ib_trigger *p_IbTrigger = container_of(work, struct t_ib_trigger, ib_trigger_work);

	if (p_IbTrigger == NULL) {
		pr_err(ITAG" IB Trigger instance is null\n");
		return;
	}

	mutex_lock(&trigger_ib_lock);

	// Input booster On/Off handling
	if (p_IbTrigger->event_type == BOOSTER_ON) {
		if (find_release_ib(p_IbTrigger->dev_type, p_IbTrigger->key_id) != NULL) {
			pr_err(ITAG" IB Trigger :: ib already exist. Key(%d)\n", p_IbTrigger->key_id);
			mutex_unlock(&trigger_ib_lock);
			return;
		}
		// Check if uniqId exits.
		do {
			uniq_id = total_ib_cnt++;

			if (total_ib_cnt == MAX_IB_COUNT)
				total_ib_cnt = 0;

		} while (!is_validate_uniqid(uniq_id));

		// Make ib instance with all needed factor.
		ib = create_ib_instance(p_IbTrigger, uniq_id);

		pr_info(ITAG" IB Trigger Press :: IB Uniq Id(%d)\n", uniq_id);

		if (ib == NULL || ib->ib_dt == NULL || ib->ib_dt->res == NULL) {
			mutex_unlock(&trigger_ib_lock);
			pr_err(ITAG" Creating ib object fail\n");
			return;
		}

		// Head time must be existed
		if (ib->ib_dt->head_time == 0) {
			mutex_unlock(&trigger_ib_lock);
			remove_ib_instance(ib);
			return;
		}

		ib->press_flag = FLAG_ON;

		// When create ib instance, insert resource info in qos list with value 0.
		for (res_type = 0; res_type < allowed_res_count; res_type++) {
			if (allowed_resources[res_type] > max_resource_count) {
				pr_err(ITAG" allow res num(%d) exceeds over max res count\n",
					allowed_resources[res_type]);
				continue;
			}
			if (ib != NULL &&
				ib->ib_dt->res[allowed_resources[res_type]].head_value != 0) {
				struct t_ib_target* tv;
				tv = kmalloc(sizeof(struct t_ib_target), GFP_KERNEL);

				if (tv == NULL)
					continue;

				tv->uniq_id = ib->uniq_id;
				tv->value = 0;

				spin_lock(&write_qos_lock);
				list_add_tail_rcu(&(tv->list), &qos_list[allowed_resources[res_type]]);
				spin_unlock(&write_qos_lock);
			}
		}
		queue_work(ib_handle_highwq, &(ib->ib_state_work[IB_HEAD]));

	} else {
		/*  Find ib instance in the list. if not, ignore this event.
		 *  if exists, Release flag on. Call ib's Release func.
		 */

		mutex_lock(&sip_rel_lock);
		ib = find_release_ib(p_IbTrigger->dev_type, p_IbTrigger->key_id);

		mutex_lock(&mem_lock);
		if (ib == NULL) {
			pr_err(ITAG" IB is null on release\n");
			mutex_unlock(&mem_lock);
			mutex_unlock(&sip_rel_lock);
			mutex_unlock(&trigger_ib_lock);
			return;
		}
		mutex_unlock(&mem_lock);

		mutex_lock(&ib->lock);
		pr_info(ITAG" IB Trigger Release :: Uniq ID(%d)\n", ib->uniq_id);
		ib->rel_flag = FLAG_ON;
		if(ib->ib_dt->tail_time == 0) {
			pr_booster(" IB tail time is 0");
			mutex_unlock(&ib->lock);
			mutex_unlock(&sip_rel_lock);
			mutex_unlock(&trigger_ib_lock);
			return;
		}

		// If head operation is already finished, tail timeout work will be triggered.
		if (ib->isHeadFinished) {
			if (!delayed_work_pending(&(ib->ib_timeout_work[IB_TAIL]))) {
				queue_delayed_work(ib_handle_highwq,
					&(ib->ib_timeout_work[IB_TAIL]),
					msecs_to_jiffies(ib->ib_dt->tail_time));
			} else {
				cancel_delayed_work(&(ib->ib_timeout_work[IB_TAIL]));
				queue_delayed_work(ib_handle_highwq,
					&(ib->ib_timeout_work[IB_TAIL]),
					msecs_to_jiffies(ib->ib_dt->tail_time));
			}
		}
		mutex_unlock(&ib->lock);
		mutex_unlock(&sip_rel_lock);
	}
	mutex_unlock(&trigger_ib_lock);

}

struct t_ib_info* create_ib_instance(struct t_ib_trigger* p_IbTrigger, int uniqId)
{
	struct t_ib_info* ib = kmalloc(sizeof(struct t_ib_info), GFP_KERNEL);
	int dev_type = p_IbTrigger->dev_type;
	int idx = 0, conv_idx = 0;

	if (ib == NULL)
		return NULL;

	ib->key_id = p_IbTrigger->key_id;
	ib->uniq_id = uniqId;
	ib->press_flag = FLAG_OFF;
	ib->rel_flag = FLAG_OFF;
	ib->isHeadFinished = 0;

	if (!(ib_mode_mask & (1 << u_ib_mode)) || !((1 <<dev_type) & ib_boost_modes[u_ib_mode].dt_mask)) {
		pr_booster("Current Ib Mode(%d) is not allowed(Masking : %0.8x, Type : %d)",
			u_ib_mode, ib_boost_modes[u_ib_mode].dt_mask, dev_type);
		if (dev_type > ib_boost_modes[0].dt_count) {
			pr_err(ITAG" dev_type(%d) is over dt count(%d)\n", dev_type, ib_boost_modes[0].dt_count);
			kfree(ib);
			ib = NULL;
			return NULL;
		}
		conv_idx = ib_boost_modes[0].type_to_idx_table[dev_type];
		ib->ib_dt = &ib_boost_modes[0].dt[dev_type];
	} else {
		conv_idx = ib_boost_modes[u_ib_mode].type_to_idx_table[dev_type];
		ib->ib_dt = &ib_boost_modes[u_ib_mode].dt[conv_idx];
	}

	INIT_WORK(&ib->ib_state_work[IB_HEAD], press_state_func);
	INIT_DELAYED_WORK(&ib->ib_timeout_work[IB_HEAD], press_timeout_func);
	INIT_WORK(&ib->ib_state_work[IB_TAIL], release_state_func);
	INIT_DELAYED_WORK(&ib->ib_timeout_work[IB_TAIL], release_timeout_func);
	mutex_init(&ib->lock);

	spin_lock(&write_ib_lock);
	list_add_tail_rcu(&(ib->list), &ib_list[dev_type]);
	spin_unlock(&write_ib_lock);

	return ib;
}

bool is_validate_uniqid(unsigned int uniq_id)
{
	int dev_type;
	int cnt = 0;
	struct t_ib_info* ib = NULL;
	rcu_read_lock();

	for (dev_type = 0; dev_type < MAX_DEVICE_TYPE_NUM; dev_type++) {
		if (list_empty(&ib_list[dev_type])) {
			pr_booster("IB List(%d) Empty", dev_type);
			continue;
		}
		list_for_each_entry_rcu(ib, &ib_list[dev_type], list) {
			cnt++;
			if (ib != NULL && ib->uniq_id == uniq_id) {
				rcu_read_unlock();
				pr_booster("uniq id find :: IB Idx(%d) old(%d) new(%d)", cnt, ib->uniq_id, uniq_id);
				return false;
			}
		}
		cnt = 0;
	}

	rcu_read_unlock();
	return true;
}

struct t_ib_info* find_release_ib(int dev_type, int key_id)
{
	struct t_ib_info* ib = NULL;
	rcu_read_lock();
	if (list_empty(&ib_list[dev_type])) {
		rcu_read_unlock();
		pr_booster("Release IB(%d) Not Exist & List Empty", key_id);
		return NULL;
	}

	list_for_each_entry_rcu(ib, &ib_list[dev_type], list) {
		if (ib != NULL && ib->key_id == key_id && ib->rel_flag == FLAG_OFF) {
			rcu_read_unlock();
			pr_booster("Release IB(%d) Found", key_id);
			return ib;
		}
	}

	rcu_read_unlock();
	pr_booster("Release IB(%d) Not Exist", key_id);
	return NULL;

}

void press_state_func(struct work_struct* work)
{

	struct t_ib_res_info res;
	struct t_ib_target* tv;
	long qos_values[MAX_RES_COUNT] = {0, };
	int res_type = 0;

	struct t_ib_info* target_ib = container_of(work, struct t_ib_info, ib_state_work[IB_HEAD]);

	pr_info(ITAG" Press State Func :::: Unique_Id(%d) Head_Time(%d)\n",
		target_ib->uniq_id, target_ib->ib_dt->head_time);

	// Get_Res_List(head) and update head value.
	for (res_type = 0; res_type < allowed_res_count; res_type++) {
		res = target_ib->ib_dt->res[allowed_resources[res_type]];
		if (res.head_value == 0)
			continue;

		// Find already added target value instance and update value as a head.
		tv = find_update_target(target_ib->uniq_id, res.res_id);

		if (tv == NULL) {
			pr_err(ITAG"Press State Func :::: %d's tv(%d) is null T.T\n",
				target_ib->uniq_id, res.res_id);
			continue;
		}

		tv->value = res.head_value;
		pr_booster("Press State Func :::: Uniq(%d)'s Update Res(%d) Head Val(%d)",
			tv->uniq_id, res.res_id, res.head_value);

		qos_values[res.res_id] = get_qos_value(res.res_id);

	}

	ib_set_booster(qos_values);

	queue_delayed_work(ib_handle_highwq, &(target_ib->ib_timeout_work[IB_HEAD]),
		msecs_to_jiffies(target_ib->ib_dt->head_time));
}

void press_timeout_func(struct work_struct* work)
{

	struct t_ib_info* target_ib = container_of(work, struct t_ib_info, ib_timeout_work[IB_HEAD].work);

	if (!target_ib)
		return;

	pr_info(ITAG" Press Timeout Func :::: Unique_Id(%d) Tail_Time(%d)\n",
		target_ib->uniq_id, target_ib->ib_dt->tail_time);

	int res_type;
	struct t_ib_res_info res;
	struct t_ib_target* tv;
	long qos_values[MAX_RES_COUNT] = {0, };
	long rel_flags[MAX_RES_COUNT] = {0, };

	mutex_lock(&sip_rel_lock);
	if (target_ib->ib_dt->tail_time != 0) {
		mutex_lock(&target_ib->lock);
		queue_work(ib_handle_highwq, &(target_ib->ib_state_work[IB_TAIL]));
		mutex_unlock(&target_ib->lock);
	} else {
		//NO TAIL Scenario : Delete Ib instance and free all memory space.
		for (res_type = 0; res_type < allowed_res_count; res_type++) {
			res = target_ib->ib_dt->res[allowed_resources[res_type]];

			tv = find_update_target(target_ib->uniq_id, res.res_id);
			if (tv == NULL) {
				pr_err(ITAG" Press Timeout Func :::: %d's TV No Exist(%d)\n",
					target_ib->uniq_id, res.res_id);
				continue;
			}

			spin_lock(&write_qos_lock);
			list_del_rcu(&(tv->list));
			spin_unlock(&write_qos_lock);
			synchronize_rcu();
			kfree(tv);
			tv = NULL;

			rcu_read_lock();
			if (!list_empty(&qos_list[res.res_id])) {
				rcu_read_unlock();
				qos_values[res.res_id] = get_qos_value(res.res_id);
				pr_booster("Press Timeout ::: Remove Val Cuz No Tail ::: Uniq(%d) Res(%d) Qos Val(%ld)",
					target_ib->uniq_id, res.res_id, qos_values[res.res_id]);
			}
			else {
				rcu_read_unlock();
				rel_flags[res.res_id] = 1;
				pr_booster("Press Timeout ::: Uniq(%d) Release Booster(%d) ::: No Tail and List Empty",
					target_ib->uniq_id, res.res_id);

			}
		}

		mutex_lock(&rel_ib_lock);
		ib_release_booster(rel_flags);
		ib_set_booster(qos_values);
		mutex_unlock(&rel_ib_lock);

		remove_ib_instance(target_ib);
	}
	mutex_unlock(&sip_rel_lock);

}

void release_state_func(struct work_struct* work)
{

	long qos_values[MAX_RES_COUNT] = {0, };
	int res_type = 0;
	struct t_ib_target* tv;
	struct t_ib_res_info res;

	struct t_ib_info* target_ib = container_of(work, struct t_ib_info, ib_state_work[IB_TAIL]);

	if (target_ib == NULL)
		return;

	mutex_lock(&target_ib->lock);

	target_ib->isHeadFinished = 1;

	pr_info(ITAG" Release State Func :::: Unique_Id(%d) Rel_Flag(%d)\n",
		target_ib->uniq_id, target_ib->rel_flag);

	for (res_type = 0; res_type < allowed_res_count; res_type++) {
		res = target_ib->ib_dt->res[allowed_resources[res_type]];
		if (res.tail_value == 0)
			continue;

		tv = find_update_target(target_ib->uniq_id, res.res_id);
		if (tv == NULL)
			continue;

		spin_lock(&write_qos_lock);
		tv->value = res.tail_value;
		spin_unlock(&write_qos_lock);

		qos_values[res.res_id] = get_qos_value(res.res_id);
		pr_booster("Release State Func :::: Uniq(%d)'s Update Tail Val (%ld), Qos_Val(%ld)",
			tv->uniq_id, tv->value, qos_values[res.res_id]);
	}

	ib_set_booster(qos_values);

	// If release event already triggered, tail delay work will be triggered after relese state func.
	if (target_ib->rel_flag == FLAG_ON) {
		if (!delayed_work_pending(&(target_ib->ib_timeout_work[IB_TAIL]))) {
			queue_delayed_work(ib_handle_highwq,
				&(target_ib->ib_timeout_work[IB_TAIL]),
				msecs_to_jiffies(target_ib->ib_dt->tail_time));
		} else {
			pr_err(ITAG" Release State Func :: tail timeout start\n");
		}
	} else {
		queue_delayed_work(ib_handle_highwq,
			&(target_ib->ib_timeout_work[IB_TAIL]),
			msecs_to_jiffies(60000));
	}

	mutex_unlock(&target_ib->lock);
}

void release_timeout_func(struct work_struct* work)
{
	long qos_values[MAX_RES_COUNT] = {0, };
	long rel_flags[MAX_RES_COUNT] = {0, };
	struct t_ib_target* tv;
	struct t_ib_res_info res;
	int res_type;

	struct t_ib_info* target_ib = container_of(work, struct t_ib_info, ib_timeout_work[IB_TAIL].work);
	if(!target_ib)
		return;

	pr_info(ITAG" Release Timeout Func :::: Unique_Id(%d)\n", target_ib->uniq_id);
	mutex_lock(&sip_rel_lock);
	for (res_type = 0; res_type < allowed_res_count; res_type++) {
		res = target_ib->ib_dt->res[allowed_resources[res_type]];

		tv = find_update_target(target_ib->uniq_id, res.res_id);
		if (tv == NULL) {
			pr_err(ITAG" Release Timeout Func :::: %d's TV No Exist(%d)\n",
				target_ib->uniq_id, res.res_id);
			continue;
		}

		pr_booster("Release Timeout Func :::: Delete Uniq(%d)'s TV Val (%ld)",
			tv->uniq_id, tv->value);

		spin_lock(&write_qos_lock);
		list_del_rcu(&(tv->list));
		spin_unlock(&write_qos_lock);
		synchronize_rcu();
		kfree(tv);
		tv = NULL;

		rcu_read_lock();
		if (!list_empty(&qos_list[res.res_id])) {
			rcu_read_unlock();
			qos_values[res.res_id] = get_qos_value(res.res_id);
			pr_booster("Release Timeout Func ::: Uniq(%d) Res(%d) Qos Val(%ld)",
				target_ib->uniq_id, res.res_id, qos_values[res.res_id]);
		}
		else {
			rcu_read_unlock();
			rel_flags[res.res_id] = 1;
			pr_booster("Release Timeout ::: Release Booster(%d's %d) ::: List Empty",
				target_ib->uniq_id, res.res_id);

		}
	}
	mutex_lock(&rel_ib_lock);
	ib_release_booster(rel_flags);
	ib_set_booster(qos_values);
	mutex_unlock(&rel_ib_lock);

	remove_ib_instance(target_ib);
	mutex_unlock(&sip_rel_lock);

}

struct t_ib_target* find_update_target(int uniq_id, int res_id)
{
	struct t_ib_target* tv;

	if (res_id < 0 || res_id >= MAX_RES_COUNT)
		return NULL;

	rcu_read_lock();
	list_for_each_entry_rcu(tv, &qos_list[res_id], list) {
		if (tv != NULL && tv->uniq_id == uniq_id) {
			rcu_read_unlock();
			return tv;
		}
	}
	rcu_read_unlock();

	return NULL;
}

long get_qos_value(int res_id)
{
	//Find tv instance that has max value in the qos_list that has the passed res_id.
	struct t_ib_target* tv;
	long ret_val = 0;

	rcu_read_lock();

	if (list_empty(&qos_list[res_id])) {
		rcu_read_unlock();
		return 0;
	}

	list_for_each_entry_rcu(tv, &qos_list[res_id], list) {
		if (tv->value > ret_val)
			ret_val = tv->value;
	}
	rcu_read_unlock();

	return ret_val;
}

void remove_ib_instance(struct t_ib_info* target_ib)
{
	struct t_ib_info* ib = NULL;
	int ib_exist = 0;

	//Check if target instance exists in the list or not.
	spin_lock(&write_ib_lock);
	list_for_each_entry_rcu(ib, &ib_list[target_ib->ib_dt->type], list) {
		if (ib != NULL && ib == target_ib) {
			ib_exist = 1;
			break;
		}
	}

	if (!ib_exist) {
		spin_unlock(&write_ib_lock);
		pr_err(ITAG" Del Ib Fail Id : %d\n", target_ib->uniq_id);
	} else {
		list_del_rcu(&(target_ib->list));
		spin_unlock(&write_ib_lock);
		synchronize_rcu();
		pr_info(ITAG" Del Ib Instance's Id : %d\n", target_ib->uniq_id);
		mutex_lock(&mem_lock);
		if (target_ib != NULL) {
			kfree(target_ib);
			target_ib = NULL;
		}
		mutex_unlock(&mem_lock);
	}
}

unsigned int create_uniq_id(int type, int code, int slot)
{
	pr_booster("Create Key Id -> type(%d), code(%d), slot(%d)", type, code, slot);
	return (type << (TYPE_BITS + CODE_BITS)) | (code << CODE_BITS) | slot;
}

void ib_auto_test(int type, int code, int val)
{
	send_ev_enable = 1;
}

//+++++++++++++++++++++++++++++++++++++++++++++++  STRUCT & VARIABLE FOR SYSFS  +++++++++++++++++++++++++++++++++++++++++++++++//
SYSFS_CLASS(enable_event, (buf, "%u\n", enable_event), 1)
SYSFS_CLASS(debug_level, (buf, "%u\n", debug_level), 1)
SYSFS_CLASS(ib_mode_state, (buf, "%u\n", ib_mode_state), 1)
SYSFS_CLASS(sendevent, (buf, "%d\n", sendevent), 3)
HEAD_TAIL_SYSFS_DEVICE(head)
HEAD_TAIL_SYSFS_DEVICE(tail)
LEVEL_SYSFS_DEVICE(level)

struct attribute* dvfs_attributes[] = {
	&dev_attr_head.attr,
	&dev_attr_tail.attr,
	&dev_attr_level.attr,
	NULL,
};

struct attribute_group dvfs_attr_group = {
	.attrs = dvfs_attributes,
};

void init_sysfs_device(struct class* sysfs_class, struct device* pdev, struct t_ib_device_tree* ib_dt) {
	struct device* sysfs_dev;
	int ret = 0;
	int bus_ret = 0;

	sysfs_dev = device_create(sysfs_class, NULL, 0, ib_dt, "%s", ib_dt->label);
	if (IS_ERR(sysfs_dev)) {
		ret = IS_ERR(sysfs_dev);
		pr_err(ITAG" Failed to create %s sysfs device[%d]\n", ib_dt->label, ret);
		return;
	}
	ret = sysfs_create_group(&sysfs_dev->kobj, &dvfs_attr_group);
	if (ret) {
		pr_err(ITAG" Failed to create %s sysfs group\n", ib_dt->label);
		return;
	}
}

int parse_dtsi_str(struct device_node *np, const char *target_node, void *target_arr, int isIntType)
{
	char prop_str[100];
	size_t prop_size = 0;
	char *prop_pointer = NULL;
	const char *token = NULL;
	int iter = 0;
	int *int_target_arr_ptr;
	char *str_target_arr_ptr;
	int copy_result;
	const char *full_str = of_get_property(np, target_node, NULL);

	if (full_str == NULL) {
		pr_err(ITAG" Target Node(%s) is null\n", target_node);
		return -1;
	}

	if (isIntType)
		int_target_arr_ptr = (int *)target_arr;
	else
		str_target_arr_ptr = (char *)target_arr;

	prop_size = strlcpy(prop_str, full_str, sizeof(char)*100);
	prop_pointer = prop_str;
	token = strsep(&prop_pointer, ",");

	while (token != NULL) {
		pr_info("%s %d's Type Value(%s)\n", target_node, iter, token);

		//Release Values inserted inside array
		if (isIntType) {
			copy_result = sscanf(token, "%d", &int_target_arr_ptr[iter]);
			if (!copy_result) {
				pr_err(ITAG" DTSI string value parsing fail\n");
				return -1;
			}
			pr_info("Target_arr[%d] : %d\n", iter, int_target_arr_ptr[iter]);
		} else {
			copy_result = sscanf(token, "%s", &str_target_arr_ptr[iter]);
			if (!copy_result) {
				pr_err(ITAG" DTSI string value parsing fail\n");
				return -1;
			}
		}

		token = strsep(&prop_pointer, ",");
		iter++;
	}

	return iter;
}

int is_ib_init_succeed(void)
{
	return (ib_trigger != NULL && ib_boost_modes != NULL &&
		ib_list != NULL && qos_list != NULL) ? 1 : 0;
}

void input_booster_exit(void)
{

	kfree(ib_trigger);
	kfree(ib_boost_modes);
	kfree(ib_device_trees);
	kfree(ib_list);
	kfree(qos_list);
	kfree(cpu_cluster_policy);
	kfree(allowed_resources);
	kfree(release_val);

	input_booster_exit_vendor();

}

// ********** Init Booster ********** //
int parse_device_info(struct device_node* np, struct t_ib_device_tree* ib_device_trees) {
	int ib_res_size = sizeof(struct t_ib_res_info);
	struct device_node* cnp;
	int device_count = 0, i;

	for_each_child_of_node(np, cnp) {
		/************************************************/
		// fill all needed data into res_info instance in dt instance.
		struct t_ib_device_tree* ib_dt = (ib_device_trees + device_count);
		struct device_node* child_resource_node;
		struct device_node* resource_node = of_find_compatible_node(cnp, NULL, "resource");

		ib_dt->res = kzalloc(ib_res_size * max_resource_count, GFP_KERNEL);
		for (i = 0; i < max_resource_count; ++i){
			ib_dt->res[i].res_id = -1;
		}

		int res_type = 0, res_id = 0;

		for_each_child_of_node(resource_node, child_resource_node) {
			int result = parse_dtsi_str(child_resource_node, "resource,id", &res_id, 1);
			pr_info(ITAG" res_id(%d) result(%d)\n", res_id, result);
			if (result == 0 || result == -1) continue;

			if (!(allowed_mask & (1<<res_id))) {
				pr_err(ITAG" res_id(%d) is not allowed res\n", res_id);
				continue;
			}

			ib_dt->res[res_id].res_id = res_id;
			ib_dt->res[res_id].label = of_get_property(child_resource_node, "resource,label", NULL);

			int inputbooster_size = 0;
			const u32* is_exist_inputbooster_size = of_get_property(child_resource_node, "resource,value", &inputbooster_size);

			if (is_exist_inputbooster_size && inputbooster_size) {
				inputbooster_size = inputbooster_size / sizeof(u32);
			}

			if (inputbooster_size != 2) {
				pr_err(ITAG" inputbooster size must be 2!\n");
				return -1; // error
			}

			for (res_type = 0; res_type < inputbooster_size; ++res_type) {
				if (res_type == IB_HEAD) {
					of_property_read_u32_index(child_resource_node, "resource,value",
						res_type, &ib_dt->res[res_id].head_value);
				}
				else if (res_type == IB_TAIL) {
					of_property_read_u32_index(child_resource_node, "resource,value",
						res_type, &ib_dt->res[res_id].tail_value);
				}
			}
		}

		ib_dt->label = of_get_property(cnp, "input_booster,label", NULL);
		pr_info(ITAG"ib_dt->label : %s\n", ib_dt->label);

		if (of_property_read_u32(cnp, "input_booster,type", &ib_dt->type)) {
			pr_err(ITAG" Failed to get type property\n");
			break;
		}
		if (of_property_read_u32(cnp, "input_booster,head_time", &ib_dt->head_time)) {
			pr_err(ITAG" Fail Get Head Time\n");
			break;
		}
		if (of_property_read_u32(cnp, "input_booster,tail_time", &ib_dt->tail_time)) {
			pr_err(ITAG" Fail Get Tail Time\n");
			break;
		}
		//Init all type of ib list.
		INIT_LIST_HEAD(&ib_list[device_count]);
		device_count++;
	}

	return device_count;
}

void input_booster_init(void)
{
	// ********** Load Frequency data from DTSI **********
	struct device_node* np;
	int i;

	int ib_dt_size = sizeof(struct t_ib_device_tree);
	int ib_res_size = sizeof(struct t_ib_res_info);
	int list_head_size = sizeof(struct list_head);
	int ddr_info_size = sizeof(struct t_ddr_info);

	int ndevice_in_dt = 0;
	int res_cnt;
	int result;

	total_ib_cnt = 0;
	ib_init_succeed = 0;
	debug_flag = 0;
	enable_event_booster = INIT_ZERO;
	max_resource_count = 0;
	allowed_res_count = 0;
	evdev_mt_slot = 0;
	trigger_cnt = 0;
	send_ev_enable = 0;

	spin_lock_init(&write_ib_lock);
	spin_lock_init(&write_qos_lock);
	mutex_init(&trigger_ib_lock);
	mutex_init(&sip_rel_lock);
	mutex_init(&rel_ib_lock);
	mutex_init(&mem_lock);

//Input Booster Trigger Strcut Init
	ib_trigger = kzalloc(sizeof(struct t_ib_trigger) * MAX_IB_COUNT, GFP_KERNEL);

	if (ib_trigger == NULL) {
		pr_err(ITAG" ib_trigger mem alloc fail\n");
		goto out;
	}

	for (i = 0; i < MAX_IB_COUNT; i++)
		INIT_WORK(&(ib_trigger[i].ib_trigger_work), trigger_input_booster);

	np = of_find_compatible_node(NULL, NULL, "input_booster");
	if (np == NULL) {
		pr_err(ITAG" Input Booster Compatible wasn't found in dtsi\n");
		goto out;
	}

// Geting the count of devices.
	ndevice_in_dt = of_get_child_count(np);

	ib_device_trees = kzalloc(ib_dt_size * ndevice_in_dt, GFP_KERNEL);
	if (ib_device_trees == NULL) {
		pr_err(ITAG" dt_infor mem alloc fail\n");
		goto out;
	}

// ib list mem alloc
	ib_list = kzalloc(list_head_size * MAX_DEVICE_TYPE_NUM, GFP_KERNEL);
	if (ib_list == NULL) {
		pr_err(ITAG" ib list mem alloc fail\n");
		goto out;
	}

// Get Needed Information from dtsi
	result = sscanf((of_get_property(np, "max_resource_count",
			NULL)), "%d", &max_resource_count);
	if (!result) {
		pr_err(ITAG"max_resource_count value parsing fail\n");
		goto out;
	}

	result = sscanf((of_get_property(np, "max_cluster_count",
			NULL)), "%d", &max_cluster_count);
	if (!result) {
		pr_err(ITAG"max_cluster_count value parsing fail\n");
		goto out;
	}

	pr_info(ITAG" resource size : %d, cluster count : %d\n",
		max_resource_count, max_cluster_count);

//qos list mem alloc
	qos_list = kzalloc(list_head_size * max_resource_count, GFP_KERNEL);
	if (qos_list == NULL) {
		pr_err(ITAG" ib list mem alloc fail\n");
		goto out;
	}
	for (res_cnt = 0; res_cnt < max_resource_count; res_cnt++)
		INIT_LIST_HEAD(&qos_list[res_cnt]);

//Init Cpu Cluster Value
	cpu_cluster_policy = kzalloc(sizeof(int) * max_cluster_count, GFP_KERNEL);
	if (cpu_cluster_policy == NULL) {
		pr_err(ITAG" cpu_cluster_policy mem alloc fail\n");
		goto out;
	}

	result = parse_dtsi_str(np, "cpu_cluster_policy", cpu_cluster_policy, 1);
	pr_info(ITAG" Init:: Total Cpu Cluster Count : %d\n", result);
	if (result < 0)
		goto out;

	if (result < max_cluster_count) {
		for (i = result; i < max_cluster_count; i++)
			cpu_cluster_policy[i] = -1;
	}

//Allow Resource
	allowed_resources = kzalloc(sizeof(int) * max_resource_count, GFP_KERNEL);
	if (allowed_resources == NULL) {
		pr_err(ITAG" allowed_resources mem alloc fail\n");
		goto out;
	}
	result = parse_dtsi_str(np, "allowed_resources", allowed_resources, 1);
	pr_info(ITAG" Init:: Total Allow Resource Count: %d\n", result);
	allowed_res_count = result;
	if (result < 0)
		goto out;

	for (i = 0; i < result; i++) {
		allowed_mask |= (1<<allowed_resources[i]);
		if (allowed_resources[i] >= max_resource_count) {
			pr_err(ITAG" allow res index exceeds over max res count %d\n",
				allowed_resources[i]);
			goto out;
		}
	}

	if (result > max_resource_count) {
		pr_err(ITAG" allow resources exceed over max resource count\n");
		goto out;
	}

//Init Resource Release Values
	release_val = kzalloc(sizeof(int) * max_resource_count, GFP_KERNEL);
	if (release_val == NULL) {
		pr_err(ITAG" release_val mem alloc fail\n");
		goto out;
	}
	result = parse_dtsi_str(np, "ib_release_values", release_val, 1);
	pr_info(ITAG" Init:: Total Release Value Count: %d\n", result);
	if (result == 0)
		goto out;

	if (result > max_resource_count) {
		pr_err(ITAG" release value parse fail :: overceed max value\n");
		goto out;
	}

// Geting the count of sub boosters.
	num_of_mode = of_get_child_count(np);
	//ib_dt_grp = kzalloc(sizeof(t_ib_device_tree*) * num_of_mode, GFP_KERNEL);
	pr_info(ITAG" %s : Total IB Mode Cnt : %d\n", __func__, num_of_mode);
	ib_boost_modes = kzalloc(sizeof(struct t_ib_boost_mode) * num_of_mode, GFP_KERNEL);

// ib_boost_modes Init
	struct device_node* cnp;
	for_each_child_of_node(np, cnp) {
		struct t_ib_device_tree* ib_dt;
		int boost_mode = 0;

		// Getting Mode Type
		result = parse_dtsi_str(cnp, "booster,mode", &boost_mode, 1);
		if (result == 0 || boost_mode < 0 || boost_mode >= num_of_mode) {
			pr_err(ITAG "Booster Mode(%d) exceeds max number of mode(%d)\n", boost_mode, num_of_mode);
			continue;
		}

		ib_boost_modes[boost_mode].type = boost_mode;
		ib_mode_mask |= (1 << boost_mode);

		//Getting Sub Booster Label
		ib_boost_modes[boost_mode].label = of_get_property(cnp, "booster,label", NULL);
		pr_info(ITAG" %s : Mode_label : %s\n", __func__, ib_boost_modes[boost_mode].label);

		ib_boost_modes[boost_mode].dt_count = of_get_child_count(cnp);
		ib_dt = ib_boost_modes[boost_mode].dt =
			kzalloc(ib_dt_size * ib_boost_modes[boost_mode].dt_count, GFP_KERNEL);
		if (ib_dt == NULL) {
			pr_err(ITAG" Mode[%d] ib_dt mem alloc fail\n", boost_mode);
			goto out;
		}

		result = parse_device_info(cnp, ib_dt);
		if (result < 0)
			goto out;

		for (i=0; i<ib_boost_modes[boost_mode].dt_count; i++) {
			//Masking all device types in current mode
			ib_boost_modes[boost_mode].dt_mask |= (1 << (ib_dt + i)->type);
			//Make table to convert type to index;
			ib_boost_modes[boost_mode].type_to_idx_table[(ib_dt + i)->type] = i;
		}

		pr_info(ITAG"%s Total Input Device Count(%d)\n", ib_boost_modes[boost_mode].label, result);
	}

	ib_init_succeed = is_ib_init_succeed();
	ib_init_succeed = input_booster_init_vendor();

	if (ib_init_succeed)
		ib_handle_highwq = alloc_workqueue("ib_unbound_high_wq", WQ_UNBOUND | WQ_HIGHPRI,
					 MAX_IB_COUNT);

out:
	// ********** Initialize Sysfs **********
	{
		struct class* sysfs_class;
		struct device* sysfs_dev;
		int ret;
		int dev_type, mode_type;
		struct t_ib_boost_mode* ib_mode;

		if (ib_init_succeed) {
			for (mode_type = 0; mode_type < num_of_mode; mode_type++) {
				ib_mode = &ib_boost_modes[mode_type];
				sysfs_class = class_create(THIS_MODULE, ib_mode->label);
				if (IS_ERR(sysfs_class)) {
					pr_err(ITAG" Failed to create class\n");
					continue;
				}
				if (!mode_type) {
					INIT_SYSFS_CLASS(enable_event)
					INIT_SYSFS_CLASS(debug_level)
					INIT_SYSFS_CLASS(ib_mode_state)
					INIT_SYSFS_CLASS(sendevent)
				}

				for (dev_type = 0; dev_type < ib_mode->dt_count; dev_type++) {
					init_sysfs_device(sysfs_class, NULL, &ib_mode->dt[dev_type]);
				}
			}
		}
	}
}
#endif //CONFIG_SEC_INPUT_BOOSTER_QC || CONFIG_SEC_INPUT_BOOSTER_SLSI || CONFIG_SEC_INPUT_BOOSTER_MTK
MODULE_LICENSE("GPL");