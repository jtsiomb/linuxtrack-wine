#ifndef LTRNP_H_
#define LTRNP_H_

int NP_GetSignature(char *sig);
int NP_QueryVersion(short *ver);

int NP_RegisterWindowHandle(void *handle);
int NP_UnregisterWindowHandle(void);
int NP_RegisterProgramProfileID(short id);

int NP_RequestData(short data);
int NP_GetData(void *data);

int NP_StopCursor(void);
int NP_StartCursor(void);

int NP_StartDataTransmission(void);
int NP_StopDataTransmission(void);

#endif	/* LTRNP_H_ */
