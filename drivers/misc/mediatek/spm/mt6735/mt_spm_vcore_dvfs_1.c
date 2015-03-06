#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/delay.h>

#include <mach/mt_vcore_dvfs.h>
#include <mach/mt_cpufreq.h>

#include "mt_spm_internal.h"

#define PER_OPP_DVS_US		(100 + 50)

#define VCORE_STA_HPM		(VCORE_STA_1)

#define get_vcore_sta()		(spm_read(SPM_SLEEP_DVFS_STA) & (VCORE_STA_1))

static const u32 vcore_dvfs_binary[] = {
	0x1840001f, 0x00000001, 0x1990001f, 0x10006b08, 0xe8208000, 0x10006b18,
	0x00000000, 0x1910001f, 0x100062c4, 0x80849001, 0x1a00001f, 0x10006b0c,
	0x1950001f, 0x10006b0c, 0xa0908805, 0xe2000002, 0x10c0041f, 0x1910001f,
	0x100062c4, 0x80809001, 0x81600801, 0xa0d59403, 0xa0d60803, 0x13000c1f,
	0xe8208000, 0x10006310, 0x0b160038, 0x1b80001f, 0x90100000, 0x88900001,
	0x10006814, 0xd8200202, 0x17c07c1f, 0x18d0001f, 0x10006b6c, 0x78a00003,
	0x0000beef, 0xd80009a2, 0x17c07c1f, 0x1900001f, 0x10006014, 0x1950001f,
	0x10006014, 0xa1508405, 0xe1000005, 0x1900001f, 0x10006814, 0xe100001f,
	0x812ab401, 0xd8000604, 0x17c07c1f, 0x1880001f, 0x10006284, 0x18d0001f,
	0x10006284, 0x80f20403, 0xe0800003, 0x80f08403, 0xe0800003, 0x1900001f,
	0x10000338, 0x1950001f, 0x10000338, 0x81730405, 0xe1000005, 0xa0c00403,
	0xe0800003, 0x1900001f, 0x10006014, 0x1950001f, 0x10006014, 0x81708405,
	0xe1000005, 0x1900001f, 0x10006b6c, 0xe100001f, 0xd0000200, 0x1910001f,
	0x10006b0c, 0x1a00001f, 0x100062c4, 0x1950001f, 0x100062c4, 0x80809001,
	0x81748405, 0xa1548805, 0x80801001, 0x81750405, 0xa1550805, 0xe2000005,
	0x1ac0001f, 0x55aa55aa, 0x1940001f, 0xaa55aa55, 0x1b80001f, 0x00001000,
	0xf0000000, 0x81441801, 0xd8200ea5, 0x17c07c1f, 0x1a00001f, 0x10006604,
	0xe2200004, 0xc0c013a0, 0x17c07c1f, 0xc0c01480, 0x17c07c1f, 0xe2200003,
	0xc0c013a0, 0x17c07c1f, 0xc0c01480, 0x17c07c1f, 0xe2200002, 0xc0c013a0,
	0x17c07c1f, 0xc0c01480, 0x17c07c1f, 0x1a00001f, 0x100062c4, 0x1890001f,
	0x100062c4, 0xa0908402, 0xe2000002, 0x1b00001f, 0x00001001, 0xf0000000,
	0x17c07c1f, 0x1a00001f, 0x100062c4, 0x1890001f, 0x100062c4, 0x80b08402,
	0xe2000002, 0x81441801, 0xd8201325, 0x17c07c1f, 0x1a00001f, 0x10006604,
	0xe2200003, 0xc0c013a0, 0x17c07c1f, 0xc0c01480, 0x17c07c1f, 0xe2200004,
	0xc0c013a0, 0x17c07c1f, 0xc0c01480, 0x17c07c1f, 0xe2200005, 0xc0c013a0,
	0x17c07c1f, 0xc0c01480, 0x17c07c1f, 0x1b00001f, 0x00000801, 0xf0000000,
	0x17c07c1f, 0x18d0001f, 0x10006604, 0x10cf8c1f, 0xd82013a3, 0x17c07c1f,
	0xf0000000, 0x17c07c1f, 0x1092041f, 0x81499801, 0xd8201605, 0x17c07c1f,
	0xd82019c2, 0x17c07c1f, 0x18d0001f, 0x40000000, 0x18d0001f, 0x60000000,
	0xd8001502, 0x00a00402, 0x814a1801, 0xd8201765, 0x17c07c1f, 0xd82019c2,
	0x17c07c1f, 0x18d0001f, 0x40000000, 0x18d0001f, 0x80000000, 0xd8001662,
	0x00a00402, 0x814a9801, 0xd82018c5, 0x17c07c1f, 0xd82019c2, 0x17c07c1f,
	0x18d0001f, 0x40000000, 0x18d0001f, 0xc0000000, 0xd80017c2, 0x00a00402,
	0xd82019c2, 0x17c07c1f, 0x18d0001f, 0x40000000, 0x18d0001f, 0x40000000,
	0xd80018c2, 0x00a00402, 0xf0000000, 0x17c07c1f
};
static struct pcm_desc vcore_dvfs_pcm = {
	.version	= "pcm_vcore_dvfs_v0.3.5.2_20150129-no_rf18",
	.base		= vcore_dvfs_binary,
	.size		= 208,
	.sess		= 1,
	.replace	= 1,
	.vec0		= EVENT_VEC(11, 1, 0, 97),	/* FUNC_26M_WAKEUP */
	.vec1		= EVENT_VEC(12, 1, 0, 127),	/* FUNC_26M_SLEEP */
};

static struct pwr_ctrl vcore_dvfs_ctrl = {
	.md1_req_mask		= 0,
	.md2_req_mask		= 0,
	.conn_mask		= 0,
};

struct spm_lp_scen __spm_vcore_dvfs = {
	.pcmdesc	= &vcore_dvfs_pcm,
	.pwrctrl	= &vcore_dvfs_ctrl,
};

char *spm_dump_vcore_dvs_regs(char *p)
{
	if (p) {
		p += sprintf(p, "SLEEP_DVFS_STA: 0x%x\n", spm_read(SPM_SLEEP_DVFS_STA));
		p += sprintf(p, "PCM_SRC_REQ   : 0x%x\n", spm_read(SPM_PCM_SRC_REQ));
		p += sprintf(p, "PCM_REG13_DATA: 0x%x\n", spm_read(SPM_PCM_REG13_DATA));
		p += sprintf(p, "PCM_REG12_MASK: 0x%x\n", spm_read(SPM_PCM_REG12_MASK));
		p += sprintf(p, "PCM_REG12_DATA: 0x%x\n", spm_read(SPM_PCM_REG12_DATA));
		p += sprintf(p, "AP_STANBY_CON : 0x%x\n", spm_read(SPM_AP_STANBY_CON));
		p += sprintf(p, "PCM_FSM_STA   : 0x%x\n", spm_read(SPM_PCM_FSM_STA));
	} else {
		spm_err("[VcoreFS] SLEEP_DVFS_STA: 0x%x\n", spm_read(SPM_SLEEP_DVFS_STA));
		spm_err("[VcoreFS] PCM_SRC_REQ   : 0x%x\n", spm_read(SPM_PCM_SRC_REQ));
		spm_err("[VcoreFS] PCM_REG13_DATA: 0x%x\n", spm_read(SPM_PCM_REG13_DATA));
		spm_err("[VcoreFS] PCM_REG12_MASK: 0x%x\n", spm_read(SPM_PCM_REG12_MASK));
		spm_err("[VcoreFS] PCM_REG12_DATA: 0x%x\n", spm_read(SPM_PCM_REG12_DATA));
		spm_err("[VcoreFS] AP_STANBY_CON : 0x%x\n", spm_read(SPM_AP_STANBY_CON));
		spm_err("[VcoreFS] PCM_FSM_STA   : 0x%x\n", spm_read(SPM_PCM_FSM_STA));
	}

	return p;
}

#define wait_pcm_complete_dvs(condition, timeout)	\
({							\
	int i = 0;					\
	while (!(condition)) {				\
		if (i >= (timeout)) {			\
			i = -EBUSY;			\
			break;				\
		}					\
		udelay(1);				\
		i++;					\
	}						\
	i;						\
})

int spm_set_vcore_dvs_voltage(unsigned int opp)
{
	int r;
	unsigned long flags;

	spin_lock_irqsave(&__spm_lock, flags);
	switch(opp) {
		case OPP_0:
			spm_write(SPM_PCM_SRC_REQ, spm_read(SPM_PCM_SRC_REQ) | SR_PCM_F26M_REQ);
			r = wait_pcm_complete_dvs(get_vcore_sta() == VCORE_STA_HPM,
					2 * PER_OPP_DVS_US /* 1.15->1.05->1.15 */);
			break;
		case OPP_1:
			spm_write(SPM_PCM_SRC_REQ, spm_read(SPM_PCM_SRC_REQ) & ~SR_PCM_F26M_REQ);
			r = 0;		/* unrequest, no need to wait */
			break;
		default:
			spm_crit("[VcoreFS] *** FAILED: OPP INDEX IS INCORRECT ***\n");
			spin_unlock_irqrestore(&__spm_lock, flags);
			return -EINVAL;
	}

	if (r >= 0) {	/* DVS pass */
		r = 0;
	} else {
		spm_dump_vcore_dvs_regs(NULL);
		BUG();	/* FIXME */
	}
	spin_unlock_irqrestore(&__spm_lock, flags);

	return r;
}

void spm_go_to_vcore_dvfs(u32 spm_flags, u32 spm_data)
{
	struct pcm_desc *pcmdesc = __spm_vcore_dvfs.pcmdesc;
	struct pwr_ctrl *pwrctrl = __spm_vcore_dvfs.pwrctrl;
	unsigned long flags;

	spin_lock_irqsave(&__spm_lock, flags);

	mt_cpufreq_set_pmic_phase(PMIC_WRAP_PHASE_NORMAL);

	set_pwrctrl_pcm_flags(pwrctrl, spm_flags);

	__spm_reset_and_init_pcm(pcmdesc);

	__spm_kick_im_to_fetch(pcmdesc);

	__spm_init_pcm_register();

	__spm_init_event_vector(pcmdesc);

	__spm_set_power_control(pwrctrl);

	__spm_set_wakeup_event(pwrctrl);

	__spm_kick_pcm_to_run(pwrctrl);

	spin_unlock_irqrestore(&__spm_lock, flags);
}

MODULE_DESCRIPTION("SPM-VCORE_DVFS Driver v0.1");