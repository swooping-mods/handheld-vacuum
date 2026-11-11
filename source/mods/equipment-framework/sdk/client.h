#ifndef MOON_EQUIPMENT_CLIENT_H
#define MOON_EQUIPMENT_CLIENT_H
#include "equipment.h"
#include "../../../runtime/native/services.h"
__declspec(dllimport) DWORD __stdcall GetModuleFileNameA(PTR, char *, DWORD);
static const MoonEquipment *me_connect(PTR domain, PTR game) {
    /* A recognized loader owns dependency startup and provider identity. A
     * missing/incompatible service fails closed instead of loading a second DLL. */
    if(moon_runtime_loader_present()){
        const MoonRuntimeApiV1 *runtime=moon_runtime_connect();
        const MoonEquipment *api=runtime?(const MoonEquipment *)runtime->find("equipment-framework","equipment",ME_ABI,sizeof(MoonEquipment)):0;
        return api&&api->abi==ME_ABI&&api->size>=sizeof(MoonEquipment)?api:0;
    }

    char path[1200];DWORD n=GetModuleFileNameA(0,path,1000);
    if(!n||n>=1000)return 0;
    while(n&&path[n-1]!='\\'&&path[n-1]!='/')n--;
    if(!n)return 0;cat(path+n,"MoonLab\\mods\\equipment-framework\\mod.dll");
    PTR module=LoadLibraryA(path);if(!module)return 0;
    int (*start)(PTR,PTR)=(int(*)(PTR,PTR))GetProcAddress(module,"MoonLab_ModStart");
    MoonEquipmentGet get=(MoonEquipmentGet)GetProcAddress(module,"MoonLab_Equipment_v1");
    if(!start||!get||!start(domain,game))return 0;
    return get(ME_ABI,sizeof(MoonEquipment));
}
#endif
