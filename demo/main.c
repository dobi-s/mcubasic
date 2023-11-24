#include "basic.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

// PC specific (for GetTickCount())
// Implement your own tickCount() depending on your hardware
#include <sysinfoapi.h>

//=============================================================================
// Functions
//=============================================================================
int sysTickMs()
{
  return GetTickCount();
};

//=============================================================================
// Main
//=============================================================================
int main()
{
  const int interval = 10;
  int       lastCall = sysTickMs();

  BasicInit();

  printf("=[ Exec ]====================================================\r\n");
  // Run task every <interval> ms
  while (1)
  {
    if (!BasicTask(interval))
      break;  // embedded systems usually don't end the task loop

    // wait until interval expired (to some other tasks)
    while (sysTickMs() - lastCall < interval)
      ;
  }
}
