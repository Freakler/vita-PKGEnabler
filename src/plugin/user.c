#include <psp2/kernel/modulemgr.h>
#include <vitasdk.h>
#include <taihen.h>

static SceUID hook;
static tai_hook_ref_t ref_hook;

int vshSysconIsIduMode_patched() {
	return 0x1;
}

void _start() __attribute__ ((weak, alias ("module_start")));

int module_start(SceSize argc, const void *args) {
	hook = taiHookFunctionImport(&ref_hook, TAI_MAIN_MODULE, TAI_ANY_LIBRARY, 0xE493EFF4, vshSysconIsIduMode_patched);
	return SCE_KERNEL_START_SUCCESS;
}

int module_stop(SceSize argc, const void *args) {
	if( hook >= 0 )
		taiHookRelease(hook, ref_hook);

	return SCE_KERNEL_STOP_SUCCESS;
}
