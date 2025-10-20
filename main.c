#include "main.h"

void main (void)
{
    SYSTEM_Init();



    /* Define a MAC Address */
    eth_addr_t ethAddr = {
            .addr[0] = 0x00,
            .addr[1] = 0x00,
            .addr[2] = 0x0c,
            .addr[3] = 0x11,
            .addr[4] = 0x11,
            .addr[5] = 0x11   };

    initLwip(ethAddr); /* Initialize LwIP with the MAC address */

    UdpEchoInit();
    TcpEchoInit();
    DoIP_Init();
    SOMEIP_Init();
    SOMEIPSD_Init();

    uint8 dstAddr[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    uint8 data[39] = "Hyundai NGV School";
    geth_sendETH(dstAddr, data, 39);


    while (1)
    {

        Ifx_Lwip_pollTimerFlags(); /* Poll LwIP timers and trigger protocols execution if required */
        Ifx_Lwip_pollReceiveFlags(); /* Receive data package through ETH */

    } /* End of while */
}
