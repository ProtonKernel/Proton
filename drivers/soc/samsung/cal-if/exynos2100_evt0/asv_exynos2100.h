#ifndef __ASV_EXYNOS9810_H__
#define __ASV_EXYNOS9810_H__

#include <linux/io.h>

#define ASV_TABLE_BASE	(0x10009000)
#define ID_TABLE_BASE	(0x10000000)

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

	unsigned chip_id;

	unsigned reserved_2:10;
	unsigned product_line:2;
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
	unsigned reserved_6;
	unsigned reserved_6_1;
	unsigned reserved_6_2;
	unsigned reserved_6_3;
	unsigned reserved_6_4;
	unsigned reserved_6_5;
	unsigned reserved_6_6;
	unsigned reserved_6_7;
	unsigned reserved_6_8;
	unsigned reserved_6_9;
	unsigned reserved_6_10:8;

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
}

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
	default:
		pr_info("Un-support ids info %d\n", id);
	}

	return ids;
}

int asv_get_table_ver(void)
{
	return asv_tbl.asv_table_version;
}

void id_get_rev(unsigned int *main_rev, unsigned int *sub_rev)
{
	*main_rev = id_tbl.main_rev;
	*sub_rev =  id_tbl.sub_rev;
}

int id_get_product_line(void)
{
	return id_tbl.product_line;
}

int id_get_asb_ver(void)
{
	return id_tbl.asb_version;
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

	pr_info("asv_table_version : %d\n", asv_tbl.asv_table_version);

	pr_info("  little cpu grp : %d\n", asv_tbl.littlecpu_asv_group);
	pr_info("  mid cpu grp : %d\n", asv_tbl.midcpu_asv_group);
	pr_info("  big cpu grp : %d\n", asv_tbl.bigcpu_asv_group);
	pr_info("  dsu grp : %d\n", asv_tbl.dsu_asv_group);
	pr_info("  g3d grp : %d\n", asv_tbl.g3d_asv_group);
	pr_info("  mif grp : %d\n", asv_tbl.mif_asv_group);
	pr_info("  sci grp : %d\n", asv_tbl.sci_asv_group);
	pr_info("  int grp : H-%d / L-%d\n", asv_tbl.int_h_asv_group, id_tbl.int_l_asv_group);
	pr_info("  cam grp : H-%d / L-%d\n", asv_tbl.cam_h_asv_group, id_tbl.cam_l_asv_group);
	pr_info("  npu grp : H-%d / L-%d\n", asv_tbl.npu_h_asv_group, id_tbl.npu_l_asv_group);
	pr_info("  cp grp : H-%d / L-%d\n", asv_tbl.cp_h_asv_group, id_tbl.cp_l_asv_group);
	pr_info("  modem grp : H-%d / L-%d\n", asv_tbl.modem_h_asv_group, id_tbl.modem_l_asv_group);

	pr_info("  product_line : %d\n", id_tbl.product_line);
	pr_info("  asb_version : %d\n", id_tbl.asb_version);

	return asv_tbl.asv_table_version;
}
#endif
