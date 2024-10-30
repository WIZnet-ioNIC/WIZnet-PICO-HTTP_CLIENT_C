/**
 * Copyright (c) 2021 WIZnet Co.,Ltd
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * ----------------------------------------------------------------------------------------------------
 * Includes
 * ----------------------------------------------------------------------------------------------------
 */
#include <stdio.h>

#include "port_common.h"

#include "wizchip_conf.h"
#include "w5x00_spi.h"

#include "dns.h"
#include "httpClient.h"
#include "timer.h"

/**
 * ----------------------------------------------------------------------------------------------------
 * Macros
 * ----------------------------------------------------------------------------------------------------
 */
/* Clock */
#define PLL_SYS_KHZ (133 * 1000)

/* Buffer */
#define ETHERNET_BUF_MAX_SIZE (1024 * 2)

/* Socket */
#define SOCKET_HTTP 0

/* Port */
#define PORT_HTTP 80

/* Retry count */
#define DNS_RETRY_COUNT 5

/**
 * ----------------------------------------------------------------------------------------------------
 * Variables
 * ----------------------------------------------------------------------------------------------------
 */
/* Network */
static wiz_NetInfo g_net_info =
    {
        .mac = {0x00, 0x08, 0xDC, 0x12, 0x34, 0x56}, // MAC address
        .ip = {192, 168, 11, 2},                     // IP address
        .sn = {255, 255, 255, 0},                    // Subnet Mask
        .gw = {192, 168, 11, 1},                     // Gateway
        .dns = {8, 8, 8, 8},                         // DNS server
        .dhcp = NETINFO_STATIC                       // DHCP enable/disable
};

/* HTTP */
static uint8_t g_http_send_buf[ETHERNET_BUF_MAX_SIZE] = {
    0,
};
static uint8_t g_http_recv_buf[ETHERNET_BUF_MAX_SIZE] = {
    0,
};

static uint8_t g_http_target_domain[] = "www.google.com";   
static uint8_t g_http_target_ip[4] = {
    0,
};

uint8_t URI[] = "/search?ei=BkGsXL63AZiB-QaJ8KqoAQ&q=W5500&oq=W5500&gs_l=psy-ab.3...0.0..6208...0.0..0.0.0.......0......gws-wiz.eWEWFN8TORw";
uint8_t flag_sent_http_request = 0;

/**
 * ----------------------------------------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------------------------------------
 */
/* Clock */
static void set_clock_khz(void);

/**
 * ----------------------------------------------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------------------------------------------
 */
int main()
{
    /* Initialize */
    uint16_t i = 0;
    uint8_t dns_retry = 0;
    uint16_t len = 0;
    uint8_t tmpbuf[2048] = {0,};

    set_clock_khz();

    stdio_init_all();

    wizchip_spi_initialize();
    wizchip_cris_initialize();

    wizchip_reset();
    wizchip_initialize();
    wizchip_check();

    network_initialize(g_net_info);

    /* Get network information */
    print_network_information(g_net_info);

    DNS_init(SOCKET_HTTP, g_http_recv_buf);

    while(1)
    {
        if (DNS_run(g_net_info.dns, g_http_target_domain, g_http_target_ip) > 0)
        {
            break;
        }
        else
        {
            dns_retry++;

            if (dns_retry <= DNS_RETRY_COUNT)
            {
                printf(" DNS timeout occurred and retry %d\n", dns_retry);
            }
        }

        if (dns_retry > DNS_RETRY_COUNT)
        {
            printf(" DNS failed\n");

            while (1)
                ;
        }

        wizchip_delay_ms(1000); // wait for 1 second
    }

    printf(" DNS success\n");
    printf(" Target domain : %s\n", g_http_target_domain);
    printf(" IP of target domain : %d.%d.%d.%d\n", g_http_target_ip[0], g_http_target_ip[1], g_http_target_ip[2], g_http_target_ip[3]);

    httpc_init(0, g_http_target_ip, PORT_HTTP, g_http_send_buf, g_http_recv_buf);

    /* Infinite loop */
    while (1)
    {
        httpc_connection_handler();
        
        if(httpc_isSockOpen && !httpc_isConnected)
        {
            httpc_connect();
        }

        if(httpc_isConnected)
        {
            if(!flag_sent_http_request)
            {
                // Send: HTTP request
                request.method = (uint8_t *)HTTP_GET;
                request.uri = (uint8_t *)URI;
                request.host = (uint8_t *)g_http_target_domain;
                
                // HTTP client example #1: Function for send HTTP request (header and body fields are integrated)
                {
                    httpc_send(&request, g_http_recv_buf, g_http_send_buf, 0); 
                }

                // HTTP client example #2: Separate functions for HTTP request - default header + body
                {
                    //httpc_send_header(&request, g_http_recv_buf, NULL, 0);
                    //httpc_send_body(g_http_send_buf, 0); // Send HTTP requset message body
                }

                // HTTP client example #3: Separate functions for HTTP request with custom header fields - default header + custom header + body
                {  
                    //httpc_add_customHeader_field(tmpbuf, "Custom-Auth", "auth_method_string"); // custom header field extended - example #1
                    //httpc_add_customHeader_field(tmpbuf, "Key", "auth_key_string"); // custom header field extended - example #2
                    //httpc_send_header(&request, g_http_recv_buf, tmpbuf, 0);
                    //httpc_send_body(g_http_send_buf, 0);
                }
                flag_sent_http_request = 1;
            }

            // Recv: HTTP response
            if(httpc_isReceived > 0)
            {
                len = httpc_recv(g_http_recv_buf, httpc_isReceived);
                printf(" >> HTTP Response - Received len: %d\r\n", len);
                printf("======================================================\r\n");
                for(i = 0; i < len; i++) 
                    printf("%c", g_http_recv_buf[i]);
                printf("\r\n");
                printf("======================================================\r\n");

                flag_sent_http_request = 0;

                sleep_ms(1000);
            }
        }
    }
}

/**
 * ----------------------------------------------------------------------------------------------------
 * Functions
 * ----------------------------------------------------------------------------------------------------
 */
/* Clock */
static void set_clock_khz(void)
{
    // set a system clock frequency in khz
    set_sys_clock_khz(PLL_SYS_KHZ, true);

    // configure the specified clock
    clock_configure(
        clk_peri,
        0,                                                // No glitchless mux
        CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS, // System PLL on AUX mux
        PLL_SYS_KHZ * 1000,                               // Input frequency
        PLL_SYS_KHZ * 1000                                // Output (must be same as no divider)
    );
}
