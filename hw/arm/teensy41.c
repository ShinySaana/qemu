#include "qemu/osdep.h"
#include "qemu/units.h"
#include "qemu/cutils.h"
#include "qapi/error.h"
#include "qemu/error-report.h"
#include "hw/arm/boot.h"
#include "hw/arm/armv7m.h"
#include "hw/or-irq.h"
#include "hw/boards.h"
#include "exec/address-spaces.h"
#include "sysemu/sysemu.h"
#include "hw/misc/unimp.h"
#include "hw/char/cmsdk-apb-uart.h"
#include "hw/timer/cmsdk-apb-timer.h"
#include "hw/timer/cmsdk-apb-dualtimer.h"
#include "hw/misc/mps2-scc.h"
#include "hw/misc/mps2-fpgaio.h"
#include "hw/ssi/pl022.h"
#include "hw/i2c/arm_sbcon_i2c.h"
#include "hw/net/lan9118.h"
#include "net/net.h"
#include "hw/watchdog/cmsdk-apb-watchdog.h"
#include "hw/qdev-clock.h"
#include "qom/object.h"
#include "qemu/osdep.h"
#include "qapi/error.h"
#include "qemu/module.h"
#include "hw/arm/boot.h"
#include "exec/address-spaces.h"
#include "hw/arm/stm32f100_soc.h"
#include "hw/qdev-properties.h"
#include "hw/qdev-clock.h"
#include "hw/misc/unimp.h"
#include "sysemu/sysemu.h"
#include "hw/char/stm32f2xx_usart.h"
#include "hw/ssi/stm32f2xx_spi.h"
#include "hw/arm/armv7m.h"
#include "qom/object.h"
#include "hw/clock.h"

#define TEENSY_41_ITCM_BASE_ADDRESS 0x00000000ULL
#define TEENSY_41_ITCM_SIZE (512 * 1024)

#define TEENSY_41_DTCM_BASE_ADDRESS 0x20000000ULL
#define TEENSY_41_DTCM_SIZE (512 * 1024)

#define TEENSY_41_RAM_BASE_ADDRESS 0x20200000ULL
#define TEENSY_41_RAM_SIZE (512 * 1024)

#define TEENSY_41_FLASH_BASE_ADDRESS 0x60000000ULL
#define TEENSY_41_FLASH_SIZE (7936 * 1024)

// The memory map documentation seems to mention two different
// cases for the ROM size, I'll just use the larger one.
#define TEENSY_41_ROM_BASE_ADDRESS 0x00200000ULL
#define TEENSY_41_ROM_SIZE (128 * 1024)

#define TEENSY_41_VTOR TEENSY_41_ROM_BASE_ADDRESS

#define TEENSY_41_AIPS1_BASE_ADDRESS 0x40000000ULL
#define TEENSY_41_AIPS1_SIZE (1024 * 1024)

#define TEENSY_41_AIPS2_BASE_ADDRESS 0x40100000ULL
#define TEENSY_41_AIPS2_SIZE (1024 * 1024)

#define TEENSY_41_AIPS3_BASE_ADDRESS 0x40200000ULL
#define TEENSY_41_AIPS3_SIZE (1024 * 1024)

#define TEENSY_41_AIPS4_BASE_ADDRESS 0x40300000ULL
#define TEENSY_41_AIPS4_SIZE (1024 * 1024)

#define TEENSY_41_CM7_PPB_BASE_ADDRESS 0xE0000000ULL
#define TEENSY_41_CM7_PPB_SIZE (1024 * 1024)

// Todo Add conditional PSRAM support?

/* Default frequency, in MHz */
#define SYSCLK_FRQ (24 * 1000 * 1000)

/* SysTick in MHz as defined by PJRC documentation */
#define REFCLK_FRQ (1 * 1000 * 1000)

#define TYPE_TEENSY_41_SOC "teensy_41_soc"
OBJECT_DECLARE_SIMPLE_TYPE(Teensy41SocState, TEENSY_41_SOC)

struct Teensy41SocState {
    /*< private >*/
    SysBusDevice parent_obj;

    /*< public >*/
    char *cpu_type;

    ARMv7MState armv7m;

    MemoryRegion flash;
    MemoryRegion itcm;
    MemoryRegion dtcm;
    MemoryRegion ram;
    MemoryRegion rom;
    MemoryRegion ocram2;
    MemoryRegion aips1;
    MemoryRegion aips2;
    MemoryRegion aips3;
    MemoryRegion aips4;
    MemoryRegion cm7_ppb;

    Clock *sysclk;
    Clock *refclk;
};

static void teensy_41_soc_initfn(Object *obj) {
    Teensy41SocState *s = TEENSY_41_SOC(obj);

    object_initialize_child(obj, "armv7m", &s->armv7m, TYPE_ARMV7M);

    s->sysclk = qdev_init_clock_in(DEVICE(s), "sysclk", NULL, NULL, 0);
    s->refclk = qdev_init_clock_in(DEVICE(s), "refclk", NULL, NULL, 0);
}

static void teensy_41_soc_realize(DeviceState *dev_soc, Error **errp)
{
    Teensy41SocState *s = TEENSY_41_SOC(dev_soc);
    DeviceState *armv7m;

    MemoryRegion *system_memory = get_system_memory();

    if (clock_has_source(s->refclk)) {
        error_setg(errp, "refclk clock must not be wired up by the board code");
        return;
    }

    if (!clock_has_source(s->sysclk)) {
        error_setg(errp, "sysclk clock must be wired up by the board code");
        return;
    }

    clock_set_source(s->refclk, s->sysclk);
    
    memory_region_init_ram(&s->itcm, NULL, "TEENSY_41.itcm", TEENSY_41_ITCM_SIZE, &error_fatal);
    if (error_fatal != NULL) {
        error_propagate(errp, error_fatal);
        return;
    }
    memory_region_add_subregion(system_memory, TEENSY_41_ITCM_BASE_ADDRESS, &s->itcm);

    memory_region_init_ram(&s->dtcm, NULL, "TEENSY_41.dtcm", TEENSY_41_DTCM_SIZE, &error_fatal);
    if (error_fatal != NULL) {
        error_propagate(errp, error_fatal);
        return;
    }
    memory_region_add_subregion(system_memory, TEENSY_41_DTCM_BASE_ADDRESS, &s->dtcm);

    memory_region_init_ram(&s->ram, NULL, "TEENSY_41.ram", TEENSY_41_RAM_SIZE, &error_fatal);
    if (error_fatal != NULL) {
        error_propagate(errp, error_fatal);
        return;
    }
    memory_region_add_subregion(system_memory, TEENSY_41_RAM_BASE_ADDRESS, &s->ram);

    memory_region_init_ram(&s->flash, NULL, "TEENSY_41.flash", TEENSY_41_FLASH_SIZE, &error_fatal);
    if (error_fatal != NULL) {
        error_propagate(errp, error_fatal);
        return;
    }
    memory_region_add_subregion(system_memory, TEENSY_41_FLASH_BASE_ADDRESS, &s->flash);

    memory_region_init_rom(&s->rom, NULL, "TEENSY_41.rom", TEENSY_41_ROM_SIZE, &error_fatal);
    if (error_fatal != NULL) {
        error_propagate(errp, error_fatal);
        return;
    }
    memory_region_add_subregion(system_memory, TEENSY_41_ROM_BASE_ADDRESS, &s->rom);



    memory_region_init_ram(&s->aips1, NULL, "TEENSY_41.aips1", TEENSY_41_AIPS1_SIZE, &error_fatal);
    if (error_fatal != NULL) {
        error_propagate(errp, error_fatal);
        return;
    }
    memory_region_add_subregion(system_memory, TEENSY_41_AIPS1_BASE_ADDRESS, &s->aips1);

    memory_region_init_ram(&s->aips2, NULL, "TEENSY_41.aips2", TEENSY_41_AIPS2_SIZE, &error_fatal);
    if (error_fatal != NULL) {
        error_propagate(errp, error_fatal);
        return;
    }
    memory_region_add_subregion(system_memory, TEENSY_41_AIPS2_BASE_ADDRESS, &s->aips2);

    memory_region_init_ram(&s->aips3, NULL, "TEENSY_41.aips3", TEENSY_41_AIPS3_SIZE, &error_fatal);
    if (error_fatal != NULL) {
        error_propagate(errp, error_fatal);
        return;
    }
    memory_region_add_subregion(system_memory, TEENSY_41_AIPS3_BASE_ADDRESS, &s->aips3);

    memory_region_init_ram(&s->aips4, NULL, "TEENSY_41.aips4", TEENSY_41_AIPS4_SIZE, &error_fatal);
    if (error_fatal != NULL) {
        error_propagate(errp, error_fatal);
        return;
    }
    memory_region_add_subregion(system_memory, TEENSY_41_AIPS4_BASE_ADDRESS, &s->aips4);

    memory_region_init_ram(&s->cm7_ppb, NULL, "TEENSY_41.cm7_ppb", TEENSY_41_CM7_PPB_SIZE, &error_fatal);
    if (error_fatal != NULL) {
        error_propagate(errp, error_fatal);
        return;
    }
    memory_region_add_subregion(system_memory, TEENSY_41_CM7_PPB_BASE_ADDRESS, &s->cm7_ppb);




    armv7m = DEVICE(&s->armv7m);
    qdev_prop_set_uint32(armv7m, "num-irq", 160);
    qdev_prop_set_uint32(armv7m, "init-nsvtor", TEENSY_41_VTOR);
    qdev_prop_set_string(armv7m, "cpu-type", s->cpu_type);
    qdev_prop_set_bit(armv7m, "enable-bitband", true);
    qdev_connect_clock_in(armv7m, "cpuclk", s->sysclk);
    qdev_connect_clock_in(armv7m, "refclk", s->refclk);
    object_property_set_link(OBJECT(&s->armv7m), "memory",
                             OBJECT(get_system_memory()), &error_abort);
    if (!sysbus_realize(SYS_BUS_DEVICE(&s->armv7m), errp)) {
        return;
    }
}

static Property teensy_41_soc_properties[] = {
    DEFINE_PROP_STRING("cpu-type", Teensy41SocState, cpu_type),
    DEFINE_PROP_END_OF_LIST(),
};

static void teensy_41_soc_class_init(ObjectClass *klass, void *data) {
    DeviceClass *dc = DEVICE_CLASS(klass);
    dc->realize = teensy_41_soc_realize;
    dc->desc = "Teensy 4.1 SoC";

    device_class_set_props(dc, teensy_41_soc_properties);
}

static const TypeInfo teensy_41_soc_info = {
    .name          = TYPE_TEENSY_41_SOC,
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(Teensy41SocState),
    .instance_init = teensy_41_soc_initfn,
    .class_init    = teensy_41_soc_class_init,
};

static void teensy_41_soc_types(void)
{
    type_register_static(&teensy_41_soc_info);
}

type_init(teensy_41_soc_types)

static void teensy_41_init(MachineState *machine) {
    DeviceState *dev;
    Clock *sysclk;

    sysclk = clock_new(OBJECT(machine), "SYSCLK");
    clock_set_hz(sysclk, SYSCLK_FRQ);


    dev = qdev_new(TYPE_TEENSY_41_SOC);
    qdev_prop_set_string(dev, "cpu-type", ARM_CPU_TYPE_NAME("cortex-m7"));
    qdev_connect_clock_in(dev, "sysclk", sysclk);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    armv7m_load_kernel(ARM_CPU(first_cpu),
                       machine->kernel_filename,
                       TEENSY_41_ROM_BASE_ADDRESS,
                       TEENSY_41_ROM_SIZE);
}

static void teensy_41_machine_init(MachineClass *mc)
{
    mc->desc = "Teensy 4.1 (Cortex-M7)";
    mc->init = teensy_41_init;
}

DEFINE_MACHINE("teensy-41", teensy_41_machine_init)
