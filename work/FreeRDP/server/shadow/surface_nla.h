/* WinHomeDesk application-account NLA. Apache-2.0. */
#ifndef WINHOME_SURFACE_NLA_H
#define WINHOME_SURFACE_NLA_H
#include <winpr/ntlm.h>

static BOOL surface_nla_hash(BYTE hash[16])
{
 const char* text = getenv("WINHOME_DESK_NT_HASH");
 if (!text || strlen(text) != 32) return FALSE;
 for (size_t i = 0; i < 16; i++) {
  unsigned int value = 0;
  for (size_t j = 0; j < 2; j++) {
   const char ch = text[2*i+j];
   unsigned int n;
   if (ch >= '0' && ch <= '9') n = (unsigned int)(ch-'0');
   else if (ch >= 'a' && ch <= 'f') n = (unsigned int)(ch-'a'+10);
   else if (ch >= 'A' && ch <= 'F') n = (unsigned int)(ch-'A'+10);
   else { SecureZeroMemory(hash,16); return FALSE; }
   value = (value << 4) | n;
  }
  hash[i] = (BYTE)value;
 }
 return TRUE;
}

static SECURITY_STATUS surface_nla_callback(void* peer, const SEC_WINNT_AUTH_IDENTITY* id,
 const SecBuffer* proof, const BYTE* randomKey, const BYTE* mic,
 const SecBuffer* micValue, BYTE* result)
{
 BYTE hash[16] = {0}; BOOL ok = FALSE;
 WINPR_UNUSED(peer); WINPR_UNUSED(proof); WINPR_UNUSED(randomKey);
 WINPR_UNUSED(mic); WINPR_UNUSED(micValue);
 if (!id || !result || !id->User || id->UserLength != 7 || id->DomainLength > 256 ||
     (id->DomainLength && !id->Domain))
  return SEC_E_LOGON_DENIED;
 if (!surface_nla_hash(hash)) return SEC_E_LOGON_DENIED;
 if (id->Flags & SEC_WINNT_AUTH_IDENTITY_UNICODE) {
#ifdef _WIN32
  if (_wcsnicmp((LPCWSTR)id->User,L"surface",7) == 0)
#else
  const WCHAR* name = (LPCWSTR)id->User;
  BOOL matches = TRUE;
  for (size_t i = 0; i < 7; i++) {
   WCHAR ch = name[i];
   if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
   if (ch != (WCHAR)"surface"[i]) matches = FALSE;
  }
  if (matches)
#endif
   ok = NTOWFv2FromHashW(hash,(LPCWSTR)id->User,id->UserLength*2,
                        (LPCWSTR)id->Domain,id->DomainLength*2,result);
 } else if (id->Flags & SEC_WINNT_AUTH_IDENTITY_ANSI) {
  if (_strnicmp((LPCSTR)id->User,"surface",7) == 0)
   ok = NTOWFv2FromHashA(hash,(LPCSTR)id->User,id->UserLength,
                        (LPCSTR)id->Domain,id->DomainLength,result);
 }
 SecureZeroMemory(hash,sizeof(hash));
 /* WinPR verifies the NTLM proof and MIC using this derived key. Returning
  * a key is not an authentication success; delegated credentials are also
  * checked by the existing PBKDF2 application authenticator. */
 return ok ? SEC_E_OK : SEC_E_LOGON_DENIED;
}
#endif
