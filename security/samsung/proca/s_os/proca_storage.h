#ifndef _LINUX_PROCA_STORAGE_H
#define _LINUX_PROCA_STORAGE_H

/* Public API for certificate storage */

/*
 * There are two options for certificates storage: xattr or database.
 * According to the selected storage type in config, the corresponding
 * implementation of API will be applied.
 */

/* Copy certificate content in cert_buff and return size of certificate */
int proca_get_certificate(struct file *file, char **cert_buff);

/* Check if certificate exists for current file */
bool proca_is_certificate_present(struct file *file);

/* Init proca Database resources in case of PROCA_CERTIFICATES_DB,
 * in case of PROCA_CERTIFICATES_XATTR init function is empty (no
 * additional initialization is required for xattr).
 */
int init_proca_storage(void);

#endif
