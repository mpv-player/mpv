/* dhahelper setup program (c) 2004 Sascha Sommer */
/* compile with gcc -o dhasetup.exe dhasetup.c    */
/* LICENSE: GPL                                   */

#include <windows.h>
#include <stdio.h>

int main(int argc,char* argv[]){
  SC_HANDLE hSCManager;
  SC_HANDLE hService;
  char path[MAX_PATH];
  printf("dhasetup (c) 2004 Sascha Sommer\n");
  GetWindowsDirectory(path,MAX_PATH);
  strcpy(path+strlen(path),"\\system32\\drivers\\dhahelper.sys");
  if(argc==1){
    printf("Usage:\n");
    printf("dhasetup install  -  Copies dhahelper.sys from the current directory to\n%s and configures it to start at boot.\n", path);
    printf("dhasetup remove  -  Removes the dhahelper utility.\n");
    return 0;
  }
  hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
  if(!strcmp(argv[1],"install")){
    printf("Installing dhahelper...\n");
    if(!CopyFile("dhahelper.sys",path,FALSE)){
      printf("Copying dhahelper.sys failed.\nEither dhahelper.sys is not in the current directory or you lack sufficient\nprivileges to write to %s.", path);
      return 1;
    }
    // Install the driver
    hService = CreateService(hSCManager,
                             "DHAHELPER",
                             "DHAHELPER",
                             SERVICE_ALL_ACCESS,
                             SERVICE_KERNEL_DRIVER,
                             SERVICE_SYSTEM_START,
                             SERVICE_ERROR_NORMAL,
                             path,
                             NULL,
                             NULL,
                             NULL,
                             NULL,
                             NULL);
    if(!hService){
      printf("Unable to register DHAHELPER Service (0x%x).\n",GetLastError());
    }
  }
  else if(!strcmp(argv[1],"remove")){
    SERVICE_STATUS ServiceStatus;
    printf("Removing dhahelper...\n");
    hService = OpenService(hSCManager, "DHAHELPER", SERVICE_ALL_ACCESS);
    ControlService(hService, SERVICE_CONTROL_STOP, &ServiceStatus);
    DeleteService(hService);
    DeleteFile(path);
  }
  else {
    printf("unknown parameter: %s\n",argv[1]);
  }
  CloseServiceHandle(hService);
  CloseServiceHandle(hSCManager);
  printf("Please reboot to let the changes take effect.\n");
  return 0;
}
