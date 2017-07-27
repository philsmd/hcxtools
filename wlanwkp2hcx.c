#define _GNU_SOURCE
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <time.h>
#include <pcap.h>
#include <sys/stat.h>
#include <stdio_ext.h>
#include "common.h"

#define WKPSIZE	2622

#define WKPESSID1	0x4c0
#define WKPESSID2	0x520
#define WKPESSID_LEN	0x540

#define WKPMAC_AP	0x514
#define WKPNONCE_AP	0x54c

#define WKPMAC_STA	0x51a
#define WKPNONCE_STA	0x56c

#define WKPKEYVER	0x544
#define WKPEAPOL_SIZE	0x548
#define WKPEAPOLDATA	0x58c
#define WKPKEYMIC	0x68c

/*===========================================================================*/
/* globale Variablen */

long int hcxcount = 0;
long int wkpcount = 0;

char *hcxoutname = NULL;
char *essidoutname = NULL;
/*===========================================================================*/
uint8_t geteapkey(uint8_t *eapdata)
{
eap_t *eap;
uint16_t keyinfo;
int eapkey = 0;

eap = (eap_t*)(uint8_t*)(eapdata);
keyinfo = (((eap->keyinfo & 0xff) << 8) | (eap->keyinfo >> 8));
if (keyinfo & WPA_KEY_INFO_ACK)
	{
	if(keyinfo & WPA_KEY_INFO_INSTALL)
		{
		/* handshake 3 */
		eapkey = 3;
		}
	else
		{
		/* handshake 1 */
		eapkey = 1;
		}
	}
else
	{
	if(keyinfo & WPA_KEY_INFO_SECURE)
		{
		/* handshake 4 */
		eapkey = 4;
		}
	else
		{
		/* handshake 2 */
		eapkey = 2;
		}
	}
return eapkey;
}
/*===========================================================================*/
int writeessid(uint8_t *essid, uint8_t essidlen)
{
FILE *fhessid = NULL;

char essidstring[34];

memset(&essidstring, 0 , 34);
memcpy(&essidstring, essid, essidlen);
if(essidoutname != NULL)
	{
	if((fhessid = fopen(essidoutname, "a")) == NULL)
		{
		fprintf(stderr, "error opening essid file %s\n", essidoutname);
		return FALSE;
		}
	}

fprintf(fhessid, "%s\n", essidstring);

if(fhessid != NULL)
	fclose(fhessid);

return TRUE;
}
/*===========================================================================*/
int writehccapx(uint8_t *wkpdata)
{
FILE *fhhcx = NULL;
hcx_t hcxrecord;
int mp;
uint8_t wkpessidlen;
uint8_t wkpeapolsize;

wkpessidlen = wkpdata[WKPESSID_LEN];
wkpeapolsize = wkpdata[WKPEAPOL_SIZE];
mp = geteapkey(&wkpdata[WKPEAPOLDATA]);

if(mp == 2)
	hcxrecord.message_pair = MESSAGE_PAIR_M12E2;
if(mp == 3)
	hcxrecord.message_pair = MESSAGE_PAIR_M32E3;
if(mp == 4)
	hcxrecord.message_pair = MESSAGE_PAIR_M14E4;

memset(&hcxrecord, 0, HCX_SIZE);
hcxrecord.signature = HCCAPX_SIGNATURE;
hcxrecord.version = HCCAPX_VERSION;
hcxrecord.message_pair = mp;
hcxrecord.essid_len = wkpessidlen;
memcpy(hcxrecord.essid, &wkpdata[WKPESSID2], wkpessidlen);

hcxrecord.keyver = wkpdata[WKPKEYVER];
memcpy(hcxrecord.mac_ap.addr, &wkpdata[WKPMAC_AP], 6);
memcpy(hcxrecord.nonce_ap, &wkpdata[WKPNONCE_AP], 32);
memcpy(hcxrecord.mac_sta.addr, &wkpdata[WKPMAC_STA], 6);
memcpy(hcxrecord.nonce_sta, &wkpdata[WKPNONCE_STA], 32);
hcxrecord.eapol_len = wkpeapolsize;
memcpy(hcxrecord.eapol, &wkpdata[WKPEAPOLDATA], wkpeapolsize +4);
memcpy(hcxrecord.keymic,  &wkpdata[WKPKEYMIC], 16);
memset(&hcxrecord.eapol[0x51], 0, 16);

if(hcxoutname != NULL)
	{
	if((fhhcx = fopen(hcxoutname, "ab")) == NULL)
		{
		fprintf(stderr, "error opening essid file %s\n", hcxoutname);
		return FALSE;
		}
	}

fwrite(&hcxrecord, 1 * HCX_SIZE, 1, fhhcx);
if(fhhcx != NULL)
	fclose(fhhcx);

hcxcount++;
return TRUE;
}
/*===========================================================================*/
int processdata(char *wkpiname)
{
struct stat statinfo;
int wkpsize;
wkpcount = 0;
uint8_t wkpessidlen = 0;
FILE *fhwkp = NULL;

char *wkpmagic = "CPWE";

uint8_t wkpdata[WKPSIZE];

if(wkpiname == NULL)
	return FALSE;

if(stat(wkpiname, &statinfo) != 0)
	{
	fprintf(stderr, "can't stat %s\n", wkpiname);
	return FALSE;
	}

if((statinfo.st_size % WKPSIZE) != 0)
	{
	fprintf(stderr, "file corrupt\n");
	return FALSE;
	}

if((fhwkp = fopen(wkpiname, "rb")) == NULL)
	{
	fprintf(stderr, "error opening file %s\n", wkpiname);
	return FALSE;
	}

wkpsize = fread(wkpdata, 1, WKPSIZE, fhwkp);
if(wkpsize != WKPSIZE)
	{
	fprintf(stderr, "error reading file %s\n", wkpiname);
	return FALSE;
	}

if(memcmp(wkpmagic, &wkpdata, 4) != 0)
	{
	fprintf(stderr, "wrong magic number %s\n", wkpiname);
	fclose(fhwkp);
	return FALSE;
	}

if(wkpsize != WKPSIZE)
	{
	fprintf(stderr, "wrong filesize %s\n", wkpiname);
	fclose(fhwkp);
	return FALSE;
	}

if(memcmp(&wkpdata[WKPESSID1], &wkpdata[WKPESSID1], 32) != 0)
	{
	fprintf(stderr, "error processing ESSID %s\n", wkpiname);
	fclose(fhwkp);
	return FALSE;
	}

wkpessidlen = wkpdata[WKPESSID_LEN];

if((wkpessidlen == 0) || (wkpessidlen > 32))
	{
	fprintf(stderr, "wrong ESSID len %s\n", wkpiname);
	fclose(fhwkp);
	return FALSE;
	}

fclose(fhwkp);
wkpcount++;

writehccapx(wkpdata);

if(essidoutname != NULL)
	writeessid(&wkpdata[WKPESSID2], wkpessidlen);

return TRUE;
}
/*===========================================================================*/
static void usage(char *eigenname)
{
printf("%s %s (C) %s ZeroBeat\n"
	"usage: %s <options> [input.wkp] [input.wkp] ...\n"
	"       %s <options> *.wkp\n"
	"\n"
	"options:\n"
	"-o <file> : output hccapx file\n"
	"-e <file> : output essidlist\n"
	"\n", eigenname, VERSION, VERSION_JAHR, eigenname, eigenname);
exit(EXIT_FAILURE);
}
/*===========================================================================*/
int main(int argc, char *argv[])
{
int index;
int auswahl;

char *eigenname = NULL;
char *eigenpfadname = NULL;

eigenpfadname = strdupa(argv[0]);
eigenname = basename(eigenpfadname);

setbuf(stdout, NULL);
while ((auswahl = getopt(argc, argv, "o:e:hv")) != -1)
	{
	switch (auswahl)
		{
		case 'o':
		hcxoutname = optarg;
		break;

		case 'e':
		essidoutname = optarg;
		break;

		default:
		usage(eigenname);
		break;
		}
	}

for (index = optind; index < argc; index++)
	{
	if(hcxoutname != NULL)
		{
		if(processdata(argv[index]) == FALSE)
			{
			fprintf(stderr, "error processing records from %s\n", argv[index]);
			exit(EXIT_FAILURE);

			}
		printf("%ld record(s) read from %s\n", wkpcount, argv[index]);
		}
	}

printf("%ld record(s) written to %s\n", hcxcount, hcxoutname);

return EXIT_SUCCESS;
}
