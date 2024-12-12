#ifndef __ASV_EXYNOS9810_H__
#define __ASV_EXYNOS9810_H__

#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/sec_debug.h>

#define ASV_TABLE_BASE	(0x10009000)
#define ID_TABLE_BASE	(0x10000000)

static struct dentry *rootdir;

struct asv_tbl_info {
	unsigned bigcpu_asv_group:4;
	unsigned midcpu_asv_group:4;
	unsigned littlecpu_asv_group:4;
	unsigned g3d_asv_group:4;
	unsigned mif_asv_group:4;
	unsigned int_h_asv_group:4;
	unsigned cam_h_asv_group:4;
	unsigned npu_h_asv_group:4;

	unsigned cp_h_asv_group:4;
	unsigned modem_h_asv_group:4;
	unsigned dsu_asv_group:4;
	unsigned sci_asv_group:4;
	unsigned sci_modify_group:4;
	unsigned dsu_modify_group:4;
	unsigned modem_h_modify_group:4;
	unsigned cp_h_modify_group:4;

	unsigned bigcpu_modify_group:4;
	unsigned midcpu_modify_group:4;
	unsigned littlecpu_modify_group:4;
	unsigned g3d_modify_group:4;
	unsigned mif_modify_group:4;
	unsigned int_h_modify_group:4;
	unsigned cam_h_modify_group:4;
	unsigned npu_h_modify_group:4;

	unsigned asv_table_version:7;
	unsigned asv_method:1;
	unsigned asb_version:7;
	unsigned vtyp_modify_enable:1;
	unsigned asvg_version:4;
	unsigned asb_pgm_reserved:12;
};

struct id_tbl_info {
	unsigned reserved_0;

	unsigned chip_id_2;

	unsigned chip_id_1:12;
	unsigned reserved_2_1:4;
	unsigned char ids_bigcpu:8;
	unsigned char ids_g3d:8;

	unsigned char ids_others:8;	/* int,cam */
	unsigned asb_version:8;
	unsigned char ids_midcpu:8;
	unsigned char ids_litcpu:8;

	unsigned char ids_mif:8;
	unsigned char ids_npu:8;
	unsigned short sub_rev:4;
	unsigned short main_rev:4;
	unsigned reserved_3:8;

	unsigned reserved_4_1:8;
	unsigned reserved_4_2:8;
	unsigned reserved_4_3:8;
	unsigned char ids_cp:8;

	unsigned int reserved_6_1;
	unsigned int reserved_6_2;
	unsigned int reserved_6_3;
	unsigned int reserved_6_4;
	unsigned int reserved_6_5;
	unsigned int reserved_6_6;
	unsigned int reserved_6_7;
	unsigned int reserved_6_8;
	unsigned int reserved_6_9;

	unsigned int_l_asv_group:4;
	unsigned cam_l_asv_group:4;
	unsigned npu_l_asv_group:4;
	unsigned cp_l_asv_group:4;
	unsigned modem_l_asv_group:4;
	unsigned reserved_7:12;

	unsigned int_l_modify_group:4;
	unsigned cam_l_modify_group:4;
	unsigned npu_l_modify_group:4;
	unsigned cp_l_modify_group:4;
	unsigned modem_l_modify_group:4;
};

struct hw_tune{
	unsigned reserved0:16;
	unsigned reserved1:8;
	unsigned code:8;
} gfx_power;

#define ASV_INFO_ADDR_CNT	(sizeof(struct asv_tbl_info) / 4)
#define ID_INFO_ADDR_CNT	(sizeof(struct id_tbl_info) / 4)

static struct asv_tbl_info asv_tbl;
static struct id_tbl_info id_tbl;

int asv_get_grp(unsigned int id)
{
	int grp = -1;

	switch (id) {
	case MIF:
		grp = asv_tbl.mif_asv_group + asv_tbl.mif_modify_group;
		break;
	case INT:
	case INTCAM:
	case MFC0:
	case MFC1:
		grp = asv_tbl.int_h_asv_group + asv_tbl.int_h_modify_group;
		break;
	case CPUCL0:
		grp = asv_tbl.littlecpu_asv_group + asv_tbl.littlecpu_modify_group;
		break;
	case CPUCL1:
		grp = asv_tbl.midcpu_asv_group + asv_tbl.midcpu_modify_group;
		break;
	case CPUCL2:
		grp = asv_tbl.bigcpu_asv_group + asv_tbl.bigcpu_modify_group;
		break;
	case G3D:
		grp = asv_tbl.g3d_asv_group + asv_tbl.g3d_modify_group;
		break;
	case NPU:
		grp = asv_tbl.npu_h_asv_group + asv_tbl.npu_h_modify_group;
		break;
	case DSU:
		grp = asv_tbl.dsu_asv_group + asv_tbl.dsu_modify_group;
		break;
	case CAM:
	case DISP:
	case AUD:
		grp = asv_tbl.cam_h_asv_group + asv_tbl.cam_h_modify_group;
		break;
	case CP_CPU:
		grp = asv_tbl.cp_h_asv_group + asv_tbl.cp_h_modify_group;
		break;
	case CP:
		grp = asv_tbl.modem_h_asv_group + asv_tbl.modem_h_modify_group;
		break;
	case INTSCI:
		grp = asv_tbl.sci_asv_group + asv_tbl.sci_modify_group;
		break;
	default:
		pr_info("Un-support asv grp %d\n", id);
	}

	return grp;
} EXPORT_SYMBOL(asv_get_grp);

int asv_get_ids_info(unsigned int id)
{
	int ids = 0;

	switch (id) {
	case G3D:
		ids = id_tbl.ids_g3d;
		break;
	case CPUCL2:
		ids = id_tbl.ids_bigcpu;
		break;
	case CPUCL1:
		ids = id_tbl.ids_midcpu;
		break;
	case CPUCL0:
		ids = id_tbl.ids_litcpu;
		break;
	case MIF:
		ids = id_tbl.ids_mif;
		break;
	case NPU:
		ids = id_tbl.ids_npu;
		break;
	case INT:
	case INTCAM:
	case MFC0:
	case MFC1:
	case CAM:
	case DISP:
	case AUD:
		ids = id_tbl.ids_others;
		break;
	case GFX:
		ids = gfx_power.code;
		break;
	default:
		pr_info("Un-support ids info %d\n", id);
	}

	return ids;
} EXPORT_SYMBOL(asv_get_ids_info);

int asv_get_table_ver(void)
{
	return asv_tbl.asv_table_version;
} EXPORT_SYMBOL(asv_get_table_ver);

void id_get_rev(unsigned int *main_rev, unsigned int *sub_rev)
{
	*main_rev = id_tbl.main_rev;
	*sub_rev =  id_tbl.sub_rev;
}

int id_get_product_line(void)
{
	return id_tbl.chip_id_1 >> 10;
} EXPORT_SYMBOL(id_get_product_line);

int id_get_asb_ver(void)
{
	return id_tbl.asb_version;
} EXPORT_SYMBOL(id_get_asb_ver);

int print_asv_table(char *buf)
{
	int r;

	r = sprintf(buf, "%s  little cpu grp : %d, ids : %d\n", buf,
			                asv_tbl.littlecpu_asv_group + asv_tbl.littlecpu_modify_group,
			                                id_tbl.ids_litcpu);
	r = sprintf(buf, "%s  mid cpu grp : %d, ids : %d\n", buf,
			                asv_tbl.midcpu_asv_group + asv_tbl.midcpu_modify_group,
			                                id_tbl.ids_midcpu);
	r = sprintf(buf, "%s  big cpu grp : %d, ids : %d\n", buf,
			                asv_tbl.bigcpu_asv_group + asv_tbl.bigcpu_modify_group,
			                                id_tbl.ids_bigcpu);
	r = sprintf(buf, "%s  dsu grp : %d\n", buf,
			                asv_tbl.dsu_asv_group + asv_tbl.dsu_modify_group);
	r = sprintf(buf, "%s  mif grp : %d, ids : %d\n", buf,
			                asv_tbl.mif_asv_group + asv_tbl.mif_modify_group,
			                                id_tbl.ids_mif);
	r = sprintf(buf, "%s  sci grp : %d\n", buf,
			                asv_tbl.sci_asv_group + asv_tbl.sci_modify_group);
	r = sprintf(buf, "%s  g3d grp : %d, ids : %d\n", buf,
			                asv_tbl.g3d_asv_group + asv_tbl.g3d_modify_group,
			                                id_tbl.ids_g3d);
	r = sprintf(buf, "%s\n", buf);
	r = sprintf(buf, "%s  int H grp : %d, ids : %d\n", buf,
			                asv_tbl.int_h_asv_group + asv_tbl.int_h_modify_group,
			                                id_tbl.ids_others);
	r = sprintf(buf, "%s  cam H grp : %d, ids : %d\n", buf,
			                asv_tbl.cam_h_asv_group + asv_tbl.cam_h_modify_group,
			                                id_tbl.ids_others);
	r = sprintf(buf, "%s  npu H grp : %d, ids : %d\n", buf,
			                asv_tbl.npu_h_asv_group + asv_tbl.npu_h_modify_group,
			                                id_tbl.ids_npu);
	r = sprintf(buf, "%s  cp H grp : %d, ids : %d\n", buf,
			asv_tbl.cp_h_asv_group + asv_tbl.cp_h_modify_group,
			id_tbl.ids_cp);
	r = sprintf(buf, "%s  modem grp : %d\n", buf,
			asv_tbl.modem_h_asv_group + asv_tbl.modem_h_modify_group);

	r = sprintf(buf, "%s  int L grp : %d, ids : %d\n", buf,
			                id_tbl.int_l_asv_group + id_tbl.int_l_modify_group,
			                                id_tbl.ids_others);
	r = sprintf(buf, "%s  cam L grp : %d, ids : %d\n", buf,
			                id_tbl.cam_l_asv_group + id_tbl.cam_l_modify_group,
			                                id_tbl.ids_others);
	r = sprintf(buf, "%s  npu L grp : %d, ids : %d\n", buf,
			                id_tbl.npu_l_asv_group + id_tbl.npu_l_modify_group,
			                                id_tbl.ids_npu);
	r = sprintf(buf, "%s  cp L grp : %d, ids : %d\n", buf,
			id_tbl.cp_l_asv_group + id_tbl.cp_l_modify_group,
			id_tbl.ids_cp);

	r = sprintf(buf, "%s\n", buf);
	r = sprintf(buf, "%s  asv_table_version : %d\n", buf, asv_tbl.asv_table_version);
	r = sprintf(buf, "%s  asb_version : %d\n", buf, id_tbl.asb_version);
	r = sprintf(buf, "%s  chipid : 0x%03x%08x\n", buf, id_tbl.chip_id_1, id_tbl.chip_id_2);
	r = sprintf(buf, "%s  main revision : %d\n", buf, id_tbl.main_rev);
	r = sprintf(buf, "%s  sub revision : %d\n", buf, id_tbl.sub_rev);
	r = sprintf(buf, "%s\n", buf);
	r = sprintf(buf, "%s  gfx power : %d\n", buf, gfx_power.code);

	return r;
}

static ssize_t
asv_info_read(struct file *filp, char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char buf[1023] = {0,};
	int r;
	r = print_asv_table(buf);

	return simple_read_from_buffer(ubuf, cnt, ppos, buf, r);
}

static const struct file_operations asv_info_fops = {
	.open		= simple_open,
	.read		= asv_info_read,
	.llseek		= seq_lseek,
};

int asv_debug_init(void)
{
	struct dentry *d;

	rootdir = debugfs_create_dir("asv", NULL);
	if (!rootdir)
		return -ENOMEM;

	d = debugfs_create_file("asv_info", 0600, rootdir, NULL, &asv_info_fops);
	if (!d)
		return -ENOMEM;

	return 0;
}

int asv_table_init(void)
{
	int i;
	unsigned int *p_table;
	unsigned int *regs;
	unsigned long tmp;

	p_table = (unsigned int *)&asv_tbl;

	for (i = 0; i < ASV_INFO_ADDR_CNT; i++) {
		exynos_smc_readsfr((unsigned long)(ASV_TABLE_BASE + 0x4 * i), &tmp);
		*(p_table + i) = (unsigned int)tmp;
	}

	p_table = (unsigned int *)&id_tbl;

	regs = (unsigned int *)ioremap(ID_TABLE_BASE, ID_INFO_ADDR_CNT * sizeof(int));
	for (i = 0; i < ID_INFO_ADDR_CNT; i++)
		*(p_table + i) = (unsigned int)regs[i];

	p_table = (unsigned int *)&gfx_power;
	regs = (unsigned int *)ioremap(ID_TABLE_BASE + 0xC308, SZ_1);
	*p_table = *regs;

	asv_debug_init();

#if IS_ENABLED(CONFIG_SEC_DEBUG)
	secdbg_exin_set_asv(asv_get_grp(CPUCL2), asv_get_grp(CPUCL1),
				asv_get_grp(CPUCL0), asv_get_grp(G3D), asv_get_grp(MIF));
	secdbg_exin_set_ids(asv_get_ids_info(CPUCL2), asv_get_ids_info(CPUCL1),
				asv_get_ids_info(CPUCL0), asv_get_ids_info(G3D));
#endif

	return asv_tbl.asv_table_version;
}
#endif
