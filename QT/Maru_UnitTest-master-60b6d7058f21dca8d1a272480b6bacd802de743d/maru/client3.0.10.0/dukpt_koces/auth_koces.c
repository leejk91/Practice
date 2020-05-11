#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "aes.h"
#include "sha256.h"
#include "hmac.h"
#include "rng.h"
#include "auth_koces.h"

#define INLENMAX        256
#define SID_LRC_IDX     1006    /* 1007번째 자리 */
#define SKey_LRC_IDX    35      /* 36번째 자리   */

static int sid_hide_idx[AP_SID_SIZE] = {959, 158, 227, 171, 212, 774, 615, 585, 813, 1009, 847, 67, 768, 327, 145, 504};
static int skey_hide_idx[AP_KEY_SIZE] = {17, 76, 348, 277, 363, 766, 658, 87, 449, 586, 825, 181, 759, 53, 896, 864};  /*  17번째는 LRC */

unsigned char KOCES_DUKPT_Calc_LRC(const unsigned char *buf, const int len)
{
    unsigned char ch, CalBCC;
    int idx = 0 ;
    
    CalBCC = buf[idx++];
    do
    {
        ch = buf[idx++] ;
        CalBCC ^= ch ;
    }
    while( idx != len );
    
    return CalBCC;
}

int KOCES_DUKPT_Verify_SID(const unsigned char *hide_data, size_t hide_data_len, unsigned char *serial_num, size_t serial_num_len)
{
    int i;
    int buf_len;
    int ret = DAMO_DUKPT_SUCCESS;
    unsigned char lrc;
    unsigned char sid[AP_SID_SIZE];
    unsigned char buf[INLENMAX];
    
    /* hide_data 길이 확인 */
    if (hide_data_len < AP_HIDE_DATA_SIZE)
        return KOCES_DUKPT_ERR_AP_HIDE_INVALID_LEN;
    
    /* SID 추출 */
    for (i=0; i<AP_SID_SIZE; i++) {
        sid[i] = hide_data[sid_hide_idx[i]-1];
    }

    buf_len = serial_num_len + AP_SID_SIZE;
    if( INLENMAX < buf_len)
        return DAMO_DUKPT_ERR_AP_MTCS_INVALID_LEN;

    memcpy(buf, serial_num, serial_num_len);
    memcpy(buf+serial_num_len, sid, AP_SID_SIZE);
    
    lrc = KOCES_DUKPT_Calc_LRC(buf, serial_num_len + AP_SID_SIZE);
    if (lrc != hide_data[1007-1])       /* 1007번째 바이트에 Serial+SID의 LRC 존재 */
        ret = KOCES_DUKPT_ERR_AP_LRC;

    memset(sid, 0x00, sizeof(sid));
    memset(buf, 0x00, buf_len);

    return ret;
}

int KOCES_DUKPT_Get_Storage_Key(const unsigned char *hide_data, size_t hide_data_len, unsigned char *skey, size_t *skey_len)
{
    int i;
    unsigned char lrc;
    
    /* hide_data 길이 확인 */
    if (hide_data_len < AP_HIDE_DATA_SIZE)
        return KOCES_DUKPT_ERR_HDATA_LEN;

    /* Storage_Key(SKey) 추출 */
    for (i=0; i<AP_KEY_SIZE; i++) {
        skey[i] = hide_data[skey_hide_idx[i]-1];
    }
    
    /* LRC Check */
    lrc = KOCES_DUKPT_Calc_LRC(skey, AP_KEY_SIZE);
    if (lrc != hide_data[SKey_LRC_IDX])
    {
        return KOCES_DUKPT_ERR_AP_LRC;
    }

    *skey_len = AP_KEY_SIZE;

    return KOCES_DUKPT_SUCCESS;
}

int KOCES_DUKPT_Get_SID(const unsigned char *hide_data, size_t hide_data_len, unsigned char *sid, size_t *sid_len)
{
    int i;
    int ret;
    unsigned char skey[AP_KEY_SIZE];
    unsigned char enc_sid[AP_SID_SIZE];
    size_t skey_len;

    /* hide_data 길이 확인 */
    if (hide_data_len < AP_HIDE_DATA_SIZE)
        return KOCES_DUKPT_ERR_HDATA_LEN;
    
    /* SID 복호화를 위해 storage_key(SKey) 얻어옴 */
    ret = KOCES_DUKPT_Get_Storage_Key(hide_data, hide_data_len, skey, &skey_len);
    if (ret < 0)
        return ret;

    /* 암호화된 SID 추출 */
    for (i=0; i<AP_SID_SIZE; i++) {
        enc_sid[i] = hide_data[sid_hide_idx[i]-1];
    }

    /* 암호화된 SID를 복호화 */
    ret = DAMO_CRYPT_AES_Decrypt(sid, sid_len, enc_sid, sizeof(enc_sid), skey, AP_SID_SIZE, AES_128, CFB_MODE);

    memset(skey, 0x00, skey_len);
    memset(enc_sid, 0x00, sizeof(enc_sid));

    return ret;
}

int KOCES_DUKPT_ReGen_Hide_Data(unsigned char *hide_data, size_t hide_data_len, unsigned char *serial_num, size_t serial_num_len)
{
    int i;
    int ret;
    size_t enc_sid_len;
    unsigned char skey[AP_KEY_SIZE];
    unsigned char enc_sid[AP_SID_SIZE];
    unsigned char sid[AP_SID_SIZE];
    
    /* hide_data 길이 확인 */
    if (hide_data_len < AP_HIDE_DATA_SIZE)
    {
        return KOCES_DUKPT_ERR_AP_HIDE_INVALID_LEN;
    }

    /* 기존 hide_data에서 SID 추출 */
    for (i=0; i<AP_SID_SIZE; i++) {
        sid[i] = hide_data[sid_hide_idx[i]-1];
    }

    /* 제품 Serial Number 와 SID가 일치하는지 LRC를 통해 확인 (일치하지 않는다면 실제로 다른 Pair이거나 이미 Convert가 완료된 상태) */
    ret = KOCES_DUKPT_Verify_SID(hide_data, hide_data_len, serial_num, serial_num_len);
    if (ret < 0) {
        return ret;
    }

    /* 신규 hide_data(Random 1024Byte) 생성(생성된 Random 중 특정 위치를 Storage_Key(SKey)로 사용 - SKey 자동 도출) */
    ret = DAMO_CRYPT_RNG(hide_data, hide_data_len);
    if (ret < 0) {
        return ret;
    }

    /* 신규로 작성된 hide_data(HData)에서 Storage_Key(SKey) 추출 */
    for (i=0; i<AP_KEY_SIZE; i++) {
        skey[i] = hide_data[skey_hide_idx[i]-1];
    }

#ifdef _DEBUG_PRINT
    {
        int i=0;
        printf("SKey(%d) = ", sizeof(skey));
        for ( i=0; i<sizeof(skey); i++)
            printf("%02X", skey[i]&0xff);
        printf("\n");
    }
#endif

    /* hide_data(HData)의 36번째 자리에 Storage_Key(SKey)의 LRC 저장 */
    hide_data[SKey_LRC_IDX] = KOCES_DUKPT_Calc_LRC(skey, AP_KEY_SIZE);

    /* 기존의 SID를 Storage_Key(SKey)로 암호화 ... */
    ret = DAMO_CRYPT_AES_Encrypt(enc_sid, &enc_sid_len, sid, sizeof(sid), skey, AP_KEY_SIZE, AES_128, CFB_MODE);
    if (ret == DAMO_CRYPT_SUCCESS) {
        /* 암호화된 SID를 hide_data내의 특정 위치에 다시 기록 */
        for (i=0; i<AP_SID_SIZE; i++) {
            hide_data[sid_hide_idx[i]-1] = enc_sid[i];
        }
    }

    memset(skey, 0x00, sizeof(skey));
    memset(enc_sid, 0x00, sizeof(enc_sid));
    memset(sid, 0x00, sizeof(sid));

    return ret;
}

int KOCES_DUKPT_Get_Temp_Key(const unsigned char *hide_data, size_t hide_data_len, const unsigned char *tid, size_t tid_len,
        unsigned char *c_random, size_t c_random_len, unsigned char *key1, size_t key1_len)
{
    int ret;
    unsigned char in[AP_SID_TID_RAND_SIZE];
    unsigned char sid[AP_SID_SIZE];
    size_t in_len;
    size_t sid_len;
    
    if(!tid || !c_random || !key1)
        return DAMO_DUKPT_ERR_AP_GTK_NULL_POINTER;
    
    if(tid_len < AP_TID_SIZE || c_random_len < AP_RAND_SIZE || key1_len < AP_KEY_SIZE)
        return DAMO_DUKPT_ERR_AP_GTK_INVALID_LEN;
    
    key1_len = AP_KEY_SIZE;
    
    ret = KOCES_DUKPT_Get_SID(hide_data, hide_data_len, sid, &sid_len);
    if (ret < 0)
        return ret;

    DAMO_CRYPT_RNG( c_random, AP_RAND_SIZE );

    in_len = AP_SID_TID_RAND_SIZE;

    memcpy(in, sid, AP_SID_SIZE);
    memcpy(in+AP_SID_SIZE, tid, AP_TID_SIZE);
    memcpy(in+AP_SID_TID_SIZE, c_random, AP_RAND_SIZE);

    if(!DAMO_CRYPT_KDF2_SHA256(in, in_len, key1, &key1_len))
    {
        ret = DAMO_DUKPT_ERR_AP_GTK_GET_TKEY_FAIL;
    }

    memset(in, 0x00, sizeof(in));
    memset(sid, 0x00, AP_SID_SIZE);

    return ret;
}

int KOCES_DUKPT_Make_TokenCS(const unsigned char *hide_data, size_t hide_data_len, const unsigned char *tid, size_t tid_len,
        unsigned char *key1, size_t key1_len, unsigned char *send_token_cs, size_t *send_token_cs_len)
{
    int ret;
    unsigned char svr_info[AP_KEY_SIZE] = {'B', '2', '0', '3', '3', 'E', 'B', 'D', '5', '8', 'D', 'A', '8', 'B', 'F', '1'};
    /* unsigned char *svr_info="B2033EBD58DA8BF1"; */
    unsigned char sid[AP_SID_SIZE] = {0x00, };
    unsigned char c_random[AP_RAND_SIZE] = {0x00, };
    size_t sid_len;

    /* HData 내에서 SID 추출 */
    ret = KOCES_DUKPT_Get_SID(hide_data, hide_data_len, sid, &sid_len);
    if (ret < 0)
    {
        return ret;
    }

    ret = KOCES_DUKPT_Get_Temp_Key(hide_data, hide_data_len, tid, tid_len, c_random, sizeof(c_random), key1, key1_len);    
    if (ret < 0)
    {
        return ret;
    }

    /* send_token_cs 생성 */
    ret = DAMO_DUKPT_Make_TokenCS(svr_info, sizeof(svr_info), tid, tid_len, c_random, sizeof(c_random), key1, key1_len, send_token_cs, send_token_cs_len);

    memset(sid, 0x00, sid_len);
    memset(c_random, 0x00, sizeof(c_random));

    if (ret < 0)
    {
        return ret;
    }

    return KOCES_DUKPT_SUCCESS;
}

int KOCES_DUKPT_Verify_TokenSC(const unsigned char *recv_token_sc, size_t recv_token_sc_len,
		const unsigned char *tid, size_t tid_len, const unsigned char *key1, size_t key1_len, unsigned char *ipek, size_t ipek_len)
{
    return DAMO_DUKPT_Verify_TokenSC(300 /*5분*/, recv_token_sc, recv_token_sc_len, tid, tid_len, key1, key1_len, ipek, ipek_len);
}

int KOCES_DUKPT_Load_Initial_Key(const unsigned char *ipek, size_t ipek_len, const unsigned char *ksn, size_t ksn_len)
{
    return DAMO_DUKPT_Load_Initial_Key(ipek, ipek_len, ksn, ksn_len);
}

int KOCES_DUKPT_Export_Future_Key_Info(const unsigned char *hide_data, size_t hide_data_len, DukptFutureKeyInfo *fki)
{
    int ret;
    unsigned char skey[AP_KEY_SIZE];
    size_t skey_len;

    ret = KOCES_DUKPT_Get_Storage_Key(hide_data, hide_data_len, skey, &skey_len);
    if (ret < 0)
        return ret;
    
    ret = DAMO_DUKPT_Export_Future_Key_Info_Ek(skey, fki);

    memset(skey, 0x00, sizeof(skey));

    return ret;
}

int KOCES_DUKPT_Import_Future_Key_Info(const unsigned char *hide_data, size_t hide_data_len, DukptFutureKeyInfo *fki)
{
    int ret;
    unsigned char skey[AP_KEY_SIZE];
    size_t skey_len;

    ret = KOCES_DUKPT_Get_Storage_Key(hide_data, hide_data_len, skey, &skey_len);
    if (ret < 0)
        return ret;
    
    ret = DAMO_DUKPT_Import_Future_Key_Info_Ek(skey, fki);

    memset(skey, 0x00, sizeof(skey));

    return ret;
}

int KOCES_DUKPT_AES_Encrypt(const unsigned char *hide_data, size_t hide_data_len, unsigned char *enc, size_t *enc_len, unsigned char *KSN, size_t *KSN_len, const unsigned char *org, size_t org_len, DukptFutureKeyInfo *fki)
{
    unsigned char pin[4] = {0x4C, };
	unsigned char account_num[16] = {0x4C, };
	DukptPinEntry pEntry;
	int flags = DAMO_DUKPT_FLAG_USE_ALL_KEY;
	int ret=0;
	size_t tmp_enc_len=0;

	/* import ksn, future key */
	if(KOCES_DUKPT_Import_Future_Key_Info(hide_data, hide_data_len, fki)<0)
		if (ret != DAMO_DUKPT_SUCCESS)
			return ret;

	ret = DAMO_DUKPT_Request_Pin_Entry(pin, sizeof(pin), account_num, sizeof(account_num), flags, &pEntry);
	if (ret != DAMO_DUKPT_SUCCESS)
		return ret;

	ret = DAMO_CRYPT_AES_Encrypt(enc, &tmp_enc_len, org, org_len, pEntry.req_enc_key, AP_KEY_SIZE, AES_128, CBC_MODE);
	if (ret != DAMO_DUKPT_SUCCESS)
		return ret;

	/* export ksn, future key */
	if(KOCES_DUKPT_Export_Future_Key_Info(hide_data, hide_data_len, fki)<0)
		if (ret != DAMO_DUKPT_SUCCESS)
			return ret;

	/* output */
	*enc_len = tmp_enc_len;
	memcpy(KSN, pEntry.ksn, 10);
	*KSN_len = 10;
    memset(&pEntry, 0x00, sizeof(pEntry));

	return DAMO_DUKPT_SUCCESS;
}

int KOCES_DUKPT_Make_Self_Protect_HASH(const unsigned char *hide_data, size_t hide_data_len,
                                      unsigned char *out, size_t *out_len, const unsigned char *in, size_t in_len)
{
    int ret;
    unsigned char skey[AP_KEY_SIZE];
    size_t skey_len;

    /* 암호화된 HASH를 위한 Key 얻어옴 */
    ret = KOCES_DUKPT_Get_Storage_Key(hide_data, hide_data_len, skey, &skey_len);
    if (ret < 0)
        return ret;

    ret = DAMO_CRYPT_AES_Encrypt(out, out_len, in, in_len, skey, skey_len, AES_128, CFB_MODE);

    memset(skey, 0x00, skey_len);

    return ret;
}

int KOCES_DUKPT_Comp_Self_Protect_HASH(const unsigned char *hide_data, size_t hide_data_len,
        const unsigned char *enc_hash, size_t enc_hash_len, const unsigned char *in_hash, size_t in_hash_len)
{
    int hash_cmp;
    int rtn;
    int ret;
    unsigned char skey[AP_KEY_SIZE];
    unsigned char dec_hash[INLENMAX];
    size_t skey_len;
    size_t dec_hash_len;
    
    /* 암호화된 HASH를 위한 Key 얻어옴 */
    ret = KOCES_DUKPT_Get_Storage_Key(hide_data, hide_data_len, skey, &skey_len);
    if (ret < 0)
        return ret;

    if (INLENMAX < enc_hash_len)
        return DAMO_DUKPT_ERR_AP_MTCS_INVALID_LEN;

    /* Enc Hash 암호해독 */
    rtn = DAMO_CRYPT_AES_Decrypt(dec_hash, &dec_hash_len, enc_hash, enc_hash_len, skey, skey_len, AES_128, CFB_MODE);
    if (rtn < 0)
    {
        ret = rtn;
    }
    else
    {
        /* HASH 길이 비교 */
        if (dec_hash_len != in_hash_len)
        {
            ret = KOCES_DUKPT_ERR_AP_HASH_LEN_COMP;
        }
        else
        {
            /* HASH 값 비교 */
            hash_cmp = memcmp(dec_hash, in_hash, in_hash_len);
            if (hash_cmp != 0)
            {
                ret = KOCES_DUKPT_ERR_AP_HASH_DIFF;
            }
        }
    }

    memset(skey, 0x00, skey_len);
    memset(dec_hash, 0x00, dec_hash_len);

    return ret;
}
