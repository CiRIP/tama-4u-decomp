/* S1C33E07 on-chip I/O registers (0x300000-0x30ffff). */
#ifndef HW_H
#define HW_H

#define REG8(a) (*(volatile u8 *)(a))
#define REG16(a) (*(volatile u16 *)(a))
#define REG32(a) (*(volatile u32 *)(a))

/*
 * The registers the firmware uses, named as in the S1C33E07 Technical Manual
 * and the Epson E07 BIOS headers without their "p" (pointer) prefix:
 * pEFSIF1_TXD -> EFSIF1_TXD. Each name has the access width the code uses;
 * a _Bn suffix marks an 8-bit access to byte n of a 16- or 32-bit register.
 */

/* Interrupt controller (ITC, 8-bit): priority levels, enables, cause flags */
#define INT_PP01L            REG8(0x300260)
#define INT_PP23L            REG8(0x300261)
#define INT_P16T23           REG8(0x300267)
#define INT_P16T45           REG8(0x300268)
#define INT_PSIO1_PAD        REG8(0x30026A)
#define INT_PRTC             REG8(0x30026B)
#define INT_PP45L            REG8(0x30026C)
#define INT_EK01_EP03        REG8(0x300270)
#define INT_EDMA             REG8(0x300271)
#define INT_E16T01           REG8(0x300272)
#define INT_E16T23           REG8(0x300273)
#define INT_E16T45           REG8(0x300274)
#define INT_ESIF01           REG8(0x300276)
#define INT_EP47_ERTC_EAD    REG8(0x300277)
#define INT_ELCDC            REG8(0x300278)
#define INT_ESIF2_ESPI       REG8(0x300279)
#define INT_FK01_FP03        REG8(0x300280)
#define INT_FDMA             REG8(0x300281)
#define INT_F16T01           REG8(0x300282)
#define INT_F16T23           REG8(0x300283)
#define INT_F16T45           REG8(0x300284)
#define INT_FSIF01           REG8(0x300286)
#define INT_FP47_FRTC_FAD    REG8(0x300287)
#define INT_FLCDC            REG8(0x300288)
#define INT_FSIF2_FSPI       REG8(0x300289)
#define IDMAREQ_R16T14       REG8(0x300291)
#define INT_PP1213L          REG8(0x3002A2)
#define INT_EP815            REG8(0x3002A6)
#define INT_EI2S             REG8(0x3002A7)
#define INT_FP815            REG8(0x3002A9)
#define INT_FI2S             REG8(0x3002AA)

/* General-purpose I/O ports (GPIO, 8-bit): data, I/O control, function select, port-input interrupt select */
#define P0_P0D               REG8(0x300380)
#define P0_IOC0              REG8(0x300381)
#define P1_P1D               REG8(0x300382)
#define P1_IOC1              REG8(0x300383)
#define P2_P2D               REG8(0x300384)
#define P2_IOC2              REG8(0x300385)
#define P3_P3D               REG8(0x300386)
#define P3_IOC3              REG8(0x300387)
#define P4_P4D               REG8(0x300388)
#define P4_IOC4              REG8(0x300389)
#define P5_P5D               REG8(0x30038A)
#define P5_IOC5              REG8(0x30038B)
#define P6_P6D               REG8(0x30038C)
#define P6_IOC6              REG8(0x30038D)
#define P7_P7D               REG8(0x30038E)
#define P8_P8D               REG8(0x300390)
#define P8_IOC8              REG8(0x300391)
#define P9_P9D               REG8(0x300392)
#define P9_IOC9              REG8(0x300393)
#define P0_47_CFP            REG8(0x3003A1)
#define P1_03_CFP            REG8(0x3003A2)
#define P7_03_CFP            REG8(0x3003AE)
#define P9_47_CFP            REG8(0x3003B3)
#define PINTSEL_SPT03        REG8(0x3003C0)
#define PINTPOL_SPP07        REG8(0x3003C2)
#define PINTEL_SEPT07        REG8(0x3003C3)
#define PINTSEL_SPT1215      REG8(0x3003C5)
#define PINTPOL_SPP815       REG8(0x3003C6)

/* A/D converter (ADC, 16-bit) */
#define AD_CLKCTL            REG16(0x300520)
#define AD_ADD               REG16(0x300540)
#define AD_TRIG_CHNL         REG16(0x300542)
#define AD_EN_SMPL_STAT      REG16(0x300544)
#define AD_CH0_BUF           REG16(0x300548)
#define AD_UPPER             REG16(0x300558)
#define AD_LOWER             REG16(0x30055A)
#define AD_CH04_INTMASK      REG16(0x30055C)
#define AD_ADVMODE           REG16(0x30055E)

/* 16-bit timers (T16, 16-bit registers; the _B0 names are 8-bit accesses to the low byte) */
#define T16_CR0A             REG16(0x300780)
#define T16_CR0B             REG16(0x300782)
#define T16_CTL0             REG16(0x300786)
#define T16_CTL0_B0          REG8(0x300786)
#define T16_CR1A             REG16(0x300788)
#define T16_CR1B             REG16(0x30078A)
#define T16_CTL1             REG16(0x30078E)
#define T16_CTL1_B0          REG8(0x30078E)
#define T16_CR2A             REG16(0x300790)
#define T16_CR2B             REG16(0x300792)
#define T16_TC2              REG16(0x300794)
#define T16_CTL2             REG16(0x300796)
#define T16_CTL2_B0          REG8(0x300796)
#define T16_CR3A             REG16(0x300798)
#define T16_CR3B             REG16(0x30079A)
#define T16_CTL3             REG16(0x30079E)
#define T16_CTL3_B0          REG8(0x30079E)
#define T16_CR4B             REG16(0x3007A2)
#define T16_TC4              REG16(0x3007A4)
#define T16_CTL4             REG16(0x3007A6)
#define T16_CTL4_B0          REG8(0x3007A6)
#define T16_CR5A             REG16(0x3007A8)
#define T16_CR5B             REG16(0x3007AA)
#define T16_CTL5             REG16(0x3007AE)
#define T16_ADVMODE          REG16(0x3007DE)
#define T16_CLKCTL_0_B0      REG8(0x3007E0)
#define T16_CLKCTL_1_B0      REG8(0x3007E2)
#define T16_CLKCTL_2         REG16(0x3007E4)
#define T16_CLKCTL_2_B0      REG8(0x3007E4)
#define T16_CLKCTL_3         REG16(0x3007E6)
#define T16_CLKCTL_3_B0      REG8(0x3007E6)
#define T16_CLKCTL_4_B0      REG8(0x3007E8)
#define T16_CLKCTL_5         REG16(0x3007EA)

/* General-purpose serial interface (EFSIO, 8-bit; ch.1 is the PN512 UART) */
#define EFSIF0_STATUS        REG8(0x300B02)
#define EFSIF1_TXD           REG8(0x300B10)
#define EFSIF1_RXD           REG8(0x300B11)
#define EFSIF1_STATUS        REG8(0x300B12)
#define EFSIF1_CTL           REG8(0x300B13)
#define EFSIF1_IRDA          REG8(0x300B14)
#define EFSIF1_BRTRUN        REG8(0x300B15)
#define EFSIF1_BRTRD         REG16(0x300B16)   /* EFSIF1_BRTRDL + EFSIF1_BRTRDM as one 16-bit access */

/* Misc registers (2): port pull-up control (8-bit) */
#define MISC_PUP0            REG8(0x300C42)
#define MISC_PUP1            REG8(0x300C43)
#define MISC_PUP3            REG8(0x300C45)
#define MISC_PUP6            REG8(0x300C48)
#define MISC_PUP7            REG8(0x300C49)
#define MISC_PUP8            REG8(0x300C4A)
#define MISC_PUP9            REG8(0x300C4B)

/* SRAM controller (SRAMC, 32-bit) */
#define SRAMC_BCLK_SETUP     REG32(0x301500)
#define SRAMC_SWAIT          REG32(0x301504)
#define SRAMC_SLV_SIZE       REG32(0x301508)
#define SRAMC_A0_BSL         REG32(0x30150C)

/* Serial peripheral interface (SPI, 32-bit) */
#define SPI_CTL1             REG32(0x301708)

/* Real-time clock (RTC, 32-bit; counters in BCD) */
#define RTCINTSTAT           REG32(0x301900)
#define RTCINTMODE           REG32(0x301904)
#define RTC_CNTL0            REG32(0x301908)
#define RTC_CNTL1            REG32(0x30190C)
#define RTCSEC               REG32(0x301910)
#define RTCMIN               REG32(0x301914)
#define RTCHOUR              REG32(0x301918)
#define RTCDAY               REG32(0x30191C)
#define RTCMONTH             REG32(0x301920)
#define RTCYEAR              REG32(0x301924)
#define RTCDAYWEEK           REG32(0x301928)

/* LCD controller (LCDC, 32-bit; only the IVRAM select is used) */
#define LCDC_IRAM            REG32(0x301A64)

/* Clock management unit (CMU, 32-bit; the _Bn names are 8-bit accesses to byte n) */
#define CMU_GATEDCLK0        REG32(0x301B00)
#define CMU_GATEDCLK1        REG32(0x301B04)
#define CMU_GATEDCLK1_B0     REG8(0x301B04)
#define CMU_GATEDCLK1_B2     REG8(0x301B06)
#define CMU_CLKCNTL          REG32(0x301B08)
#define CMU_PLL              REG32(0x301B0C)
#define CMU_OPT              REG32(0x301B14)
#define CMU_PROTECT          REG32(0x301B24)

#endif
