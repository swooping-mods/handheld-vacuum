/* Optional same-process shop activation. Owning mods retain prices, authority,
 * persistence and all standalone entry points. Call poll only on Unity Update.
 * Win32 auto-reset events carry no identity or payment authorization. */
#ifndef MOONLAB_SHOP_SIGNAL_H
#define MOONLAB_SHOP_SIGNAL_H
static struct {
    PTR ( __stdcall *create)(PTR,BOOL,BOOL,const char *);
    DWORD ( __stdcall *wait)(PTR,DWORD);
    BOOL ( __stdcall *close)(PTR);
    PTR handle;
} moon_shop_signal;
static int moon_shop_listen(const char *item){
    PTR k=GetModuleHandleA("kernel32.dll");
    moon_shop_signal.create=(PTR(__stdcall *)(PTR,BOOL,BOOL,const char *))GetProcAddress(k,"CreateEventA");
    moon_shop_signal.wait=(DWORD(__stdcall *)(PTR,DWORD))GetProcAddress(k,"WaitForSingleObject");
    moon_shop_signal.close=(BOOL(__stdcall *)(PTR))GetProcAddress(k,"CloseHandle");
    if(!moon_shop_signal.create||!moon_shop_signal.wait||!moon_shop_signal.close)return 0;
    char name[160],*p=cat(name,"Local\\MoonLabShop.");p=num(p,GetCurrentProcessId());p=cat(p,".");cat(p,item);
    moon_shop_signal.handle=moon_shop_signal.create(0,0,0,name);return moon_shop_signal.handle!=0;
}
static int moon_shop_poll(void){return moon_shop_signal.handle&&moon_shop_signal.wait(moon_shop_signal.handle,0)==0;}
static void moon_shop_unlisten(void){if(moon_shop_signal.handle)moon_shop_signal.close(moon_shop_signal.handle);moon_shop_signal.handle=0;}
#endif
