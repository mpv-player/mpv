/*dhahelper setup program (c) 2004 Sascha Sommer*/
/*compile with gcc -o dhasetup.exe dhasetup.c      */
/*LICENSE: GPL                                                 */

#include <windows.h>
#include <stdio.h>

int main(int argc,char* argv[]){
  SC_HANDLE hSCManager;
  SC_HANDLE hService;
  printf("dhasetup (c) 2004 Sascha Sommer\n");
  if(argc==1){
    printf("usage:\n");
    printf("dhasetup install  -  copys dhahelper.sys from the current dir to windows/system32/drivers and configures it to start at system start\n");
    printf("dhasetup remove  -  removes the dhahelper util\n");
    return 0;
  }
  hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  
  if(!strcmp(argv[1],"install")){
    printf("installing dhahelper\n");
    CopyFile("dhahelper.sys","c:\\windows\\System32\\drivers\\dhahelper.sys",FALSE);
    // Install the driver
    hService = CreateService(hSCManager,
                             "DHAHELPER",
                             "DHAHELPER",
                             SERVICE_ALL_ACCESS,
                             SERVICE_KERNEL_DRIVER,
                             SERVICE_SYSTEM_START,
                             SERVICE_ERROR_NORMAL,
                             "c:\\windows\\System32\\drivers\\dhahelper.sys",
                             NULL,
                             NULL,
                             NULL,
                             NULL,
                             NULL);
  }
  else if(!strcmp(argv[1],"remove")){
    SERVICE_STATUS ServiceStatus;
    printf("removing dhahelper\n");
    hService = OpenService(hSCManager, "DHAHELPER", SERVICE_ALL_ACCESS);
    ControlService(hService, SERVICE_CONTROL_STOP, &ServiceStatus);
    DeleteService(hService);
    DeleteFile("c:\\windows\\System32\\drivers\\dhahelper.sys");
  }
  else {
    printf("unknown parameter: %s\n",argv[1]);
  }
  CloseServiceHandle(hService);
  CloseServiceHandle(hSCManager);
  printf("please reboot to let the changes take effect\n");
  return 0;
}
