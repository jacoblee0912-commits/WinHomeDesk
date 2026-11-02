using System;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;

// NT hash is used only by the child engine's application-account NLA provider.
// It is never written to connection files or settings.
public static class AppCredentials {
 [DllImport("bcrypt.dll",CharSet=CharSet.Unicode)] static extern int BCryptOpenAlgorithmProvider(out IntPtr algorithm,string name,string implementation,uint flags);
 [DllImport("bcrypt.dll")] static extern int BCryptHash(IntPtr algorithm,IntPtr secret,int secretSize,byte[] input,int inputSize,byte[] output,int outputSize);
 [DllImport("bcrypt.dll")] static extern int BCryptCloseAlgorithmProvider(IntPtr algorithm,uint flags);
 public static string NtlmHash(string password) {
  IntPtr algorithm=IntPtr.Zero;byte[] input=Encoding.Unicode.GetBytes(password),hash=new byte[16];
  try {
   if(BCryptOpenAlgorithmProvider(out algorithm,"MD4",null,0)<0 || BCryptHash(algorithm,IntPtr.Zero,0,input,input.Length,hash,hash.Length)<0)
    throw new CryptographicException("NLA 인증 키를 만들 수 없습니다.");
   return BitConverter.ToString(hash).Replace("-","");
  } finally {Array.Clear(input,0,input.Length);Array.Clear(hash,0,hash.Length);if(algorithm!=IntPtr.Zero)BCryptCloseAlgorithmProvider(algorithm,0);}
 }
}
