#ifdef __CYGWIN__
#include <stdint.h>
#define __int64 uint64_t
#endif

#ifdef WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include <stdlib.h>
#include "natpmp.h"

#include "fr_free_miniupnp_libnatpmp_NatPmp.h"

#ifdef __cplusplus
extern "C" {
#endif

JNIEXPORT void JNICALL Java_fr_free_miniupnp_libnatpmp_NatPmp_init (JNIEnv *env, jobject obj, jint forcegw, jint forcedgw) {
  natpmp_t *p = malloc (sizeof(natpmp_t));
  if (p == NULL) return;

  initnatpmp(p, forcegw, (in_addr_t) forcedgw);

  jobject wrapped =  (*env)->NewDirectByteBuffer(env, p, sizeof(natpmp_t));
  if (wrapped == NULL) return;

  jclass thisClass = (*env)->GetObjectClass(env,obj);
  if (thisClass == NULL) return;

  jfieldID fid = (*env)->GetFieldID(env, thisClass, "natpmp", "Ljava/nio/ByteBuffer;");
  if (fid == NULL) return;
  (*env)->SetObjectField(env, obj, fid, wrapped);
}

JNIEXPORT void JNICALL Java_fr_free_miniupnp_libnatpmp_NatPmp_free (JNIEnv *env, jobject obj) {

  jclass thisClass = (*env)->GetObjectClass(env,obj);
  if (thisClass == NULL) return;

  jfieldID fid = (*env)->GetFieldID(env, thisClass, "natpmp", "Ljava/nio/ByteBuffer;");

  if (fid == NULL) return;
  jobject wrapped = (*env)->GetObjectField(env, obj, fid);
  if (wrapped == NULL) return;

  natpmp_t* natpmp = (natpmp_t*) (*env)->GetDirectBufferAddress(env, wrapped);

  closenatpmp(natpmp);

  if (natpmp == NULL) return;
  free(natpmp);

  (*env)->SetObjectField(env, obj, fid, NULL);
}

static natpmp_t* getNatPmp(JNIEnv* env, jobject obj) {
  jclass thisClass = (*env)->GetObjectClass(env,obj);
  if (thisClass == NULL) return NULL;

  jfieldID fid = (*env)->GetFieldID(env, thisClass, "natpmp", "Ljava/nio/ByteBuffer;");

  if (fid == NULL) return NULL;
  jobject wrapped = (*env)->GetObjectField(env, obj, fid);
  if (wrapped == NULL) return NULL;

  natpmp_t* natpmp = (natpmp_t*) (*env)->GetDirectBufferAddress(env, wrapped);

  return natpmp;
}

JNIEXPORT jint JNICALL Java_fr_free_miniupnp_libnatpmp_NatPmp_sendPublicAddressRequest(JNIEnv* env, jobject obj) {
  natpmp_t* natpmp = getNatPmp(env, obj);
  if (natpmp == NULL) return -1;

  return sendpublicaddressrequest(natpmp);
}


JNIEXPORT void JNICALL Java_fr_free_miniupnp_libnatpmp_NatPmp_startup(JNIEnv* env, jclass cls) {
  (void)env;
  (void)cls;
#ifdef WIN32
  WSADATA wsaData;
  WORD wVersionRequested = MAKEWORD(2, 2);
  WSAStartup(wVersionRequested, &wsaData);
#endif
}


JNIEXPORT jint JNICALL Java_fr_free_miniupnp_libnatpmp_NatPmp_sendNewPortMappingRequest(JNIEnv* env, jobject obj, jint protocol, jint privateport, jint publicport, jint lifetime) {
  natpmp_t* natpmp = getNatPmp(env, obj);
  if (natpmp == NULL) return -1;

  return sendnewportmappingrequest(natpmp, protocol, privateport, publicport, lifetime);
}

JNIEXPORT jlong JNICALL Java_fr_free_miniupnp_libnatpmp_NatPmp_getNatPmpRequestTimeout(JNIEnv* env, jobject obj) {
  natpmp_t* natpmp = getNatPmp(env, obj);

  struct timeval timeout;

  getnatpmprequesttimeout(natpmp, &timeout);

  return ((jlong) timeout.tv_sec) * 1000 + (timeout.tv_usec / 1000);

}

#define SET_FIELD(prefix, name, type, longtype) {                  \
  jfieldID fid = (*env)->GetFieldID(env, thisClass, #name, type); \
  if (fid == NULL) return -1; \
  (*env)->Set ## longtype ## Field(env, response, fid, resp. prefix name);     \
}

JNIEXPORT jint JNICALL Java_fr_free_miniupnp_libnatpmp_NatPmp_readNatPmpResponseOrRetry(JNIEnv* env, jobject obj, jobject response) {

  natpmp_t* natpmp = getNatPmp(env, obj);
  natpmpresp_t resp;
  int result = readnatpmpresponseorretry(natpmp, &resp);

  if (result != 0) {
    return result;
  }

  jclass thisClass = (*env)->GetObjectClass(env, response);
  if (thisClass == NULL) return -1;

  SET_FIELD(,type, "S", Short);
  SET_FIELD(,resultcode, "S", Short);

  jfieldID fid = (*env)->GetFieldID(env, thisClass, "epoch", "J");
  if (fid == NULL) return -1;
  (*env)->SetLongField(env, response, fid, ((jlong)resp.epoch) * 1000);

  if (resp.type == 0) {
  jfieldID fid = (*env)->GetFieldID(env, thisClass, "addr", "I");
  if (fid == NULL) return -1;
  (*env)->SetIntField(env, response, fid, resp.pnu.publicaddress.addr.s_addr);


  } else {
    SET_FIELD(pnu.newportmapping., privateport, "I", Int);
    SET_FIELD(pnu.newportmapping., mappedpublicport, "I", Int);

    jfieldID fid = (*env)->GetFieldID(env, thisClass, "lifetime", "J");
    if (fid == NULL) return -1;
    (*env)->SetLongField(env, response, fid, ((jlong) resp.pnu.newportmapping.lifetime) * 1000 * 1000);
  }
  return result;
}


#ifdef __cplusplus
}
#endif
