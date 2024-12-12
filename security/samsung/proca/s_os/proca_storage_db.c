#include "proca_certificate_db.h"
#include "proca_storage.h"

int proca_get_certificate(struct file *file, char **cert_buff)
{
	int ret = 0;

	ret = proca_get_certificate_db(file, cert_buff);
	return ret;
}

bool proca_is_certificate_present(struct file *file)
{
	return proca_is_certificate_present_db(file);
}

int __init init_proca_storage(void)
{
	int ret = 0;

	ret = proca_keyring_init();
	if (ret)
		return ret;

	ret = proca_load_built_x509();
	if (ret)
		return ret;

	ret = proca_certificate_db_init();
	if (ret)
		return ret;

#ifdef CONFIG_PROCA_CERT_DEVICE
	ret = init_proca_cert_device();
#endif

	return ret;
}
