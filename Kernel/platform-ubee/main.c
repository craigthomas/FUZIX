#include <kernel.h>
#include <timer.h>
#include <kdata.h>
#include <printf.h>
#include <devtty.h>
#include <ubee.h>

uint16_t ramtop = PROGTOP;

/* On idle we spin checking for the terminals. Gives us more responsiveness
   for the polled ports */
void platform_idle(void)
{
	__asm halt __endasm;
}

void do_beep(void)
{
}

static uint8_t has_rtc;

__sfr __at 0x02 pia0b;
__sfr __at 0x04 cmos_reg;
__sfr __at 0x07 cmos_read;

uint8_t rtc_secs(void)
{
	cmos_reg = 0x00;
	return cmos_read;
}

void has_cmos_rtc(void)
{
	/* See if the week looks valid - probably want a stronger check */
	cmos_reg = 0x06;
	if (cmos_read == 0 || cmos_read > 7)
		panic("RTC required");
}

void platform_interrupt(void)
{
	static uint8_t icount;
	uint8_t r = pia0b;
	/* TODO: printer interrupt */
	/* Need to check if TC */
	if (r & 0x02)
		kbd_interrupt();
	if (r & 0x80) {
		cmos_reg = 0x0C;
		if (cmos_read & 0x40) {
			icount++;
			timer_interrupt();
			/* Turn 8Hz into 10Hz */
			if (icount == 4) {
				timer_interrupt();
				icount = 0;
			}
		}
	}
}

void map_init(void)
{

}

uint8_t disk_type[2];

/* More _DISCARD material */

__sfr __at 0x58 portswitch;
__sfr __at 0x45 probe_reg;
__sfr __at 0x41 fdc_cyl;
__sfr __at 0x48 fdc_stat;

void do_diskprobe(uint8_t *p)
{
	probe_reg = 0x5A;
	/* probe_reg is read/write same value for both controllers so if it
	   doesn't work we have no disk */
	if (probe_reg != 0x5A)
		return;
	/* If it mirrored check if a second value mirrors */
	if (fdc_cyl == 0x5A) {
		probe_reg = 0x22;
		if (fdc_cyl == 0x22) {
			/* It's a floppy */
			if (fdc_stat & 0x0F)
				*p = DISK_TYPE_FDC_D;	/* Dream disc */
			else
				*p = DISK_TYPE_FDC;
			return;
		}
	}
	/* Double check */
	probe_reg = 0x22;
	if (probe_reg != 0x22)
		return;
	*p = DISK_TYPE_HDC;
}

static const char dcstr[] = "Disk controller %d: %s\n";
static const char *diskname[4] = { NULL, "2793", "DreamDisc", "WD1010" };

void diskprobe(void)
{
	portswitch = 0;
	do_diskprobe(disk_type);
	portswitch = 1;
	do_diskprobe(disk_type + 1);
	/* Hard case: two of the same */
	if (disk_type[0] == disk_type[1]) {
		/* No disk at all - we don't care if port 58 works */
		if (!disk_type[0])
			return;
		/* We might have internal and external the same ? */
		probe_reg = 0x5C;
		portswitch = 0;
		probe_reg = 0x00;
		portswitch = 1;
		if (probe_reg != 0x5C)
			disk_type[1] = 0;
	}
	kprintf(dcstr, 0, diskname[disk_type[0]]);
	if (disk_type[1])
		kprintf(dcstr, 1, diskname[disk_type[1]]);
}


/* The bank bits are not laid out nicely for historical reasons

   A classic uBee has the bank selection in bits 0/1
   The 256TC needed an extra bit so reused bit 5 (rom selector)
   The 3rd party 512K/1MB add ons used bits 7/6 for the high bank bits

   Bank 0/0 holds the kernel and common Bank 0/1 holds the kernel low 32k */
static uint8_t map_mod(uint8_t banknum)
{
	uint8_t r = banknum & 0x03;
	r |= 0xC;		/* ROM off, Video off */
	if (ubee_model == UBEE_256TC)
		r |= (banknum & 4) ? 0x20 : 0;
	else
		r |= (banknum & 0x0C) << 4;
	return r;
}

/* Kernel in bank 0 low/high,user apps in bank 1 low/high (and if present
   other banks too). Memorywise it's a lot like the TRS80 layout */

void pagemap_init(void)
{
	uint8_t i;
	uint8_t nbank = procmem / 32;

	/* Just a handy spot to run it early */
	has_cmos_rtc();

	for (i = 1; i < nbank; i++)
		pagemap_add(map_mod(i));

}

uint8_t platform_param(char *p)
{
	return 0;
}
