using System;
public static class TestCredentials026 {
 public static void Main(){
  if(AppCredentials.NtlmHash("password")!="8846F7EAEE8FB117AD06BDD830B7586C")throw new Exception("NTLM known vector mismatch");
  if(AppCredentials.NtlmHash("")!="31D6CFE0D16AE931B73C59D7E0C089C0")throw new Exception("empty vector mismatch");
  Console.WriteLine("PASS CNG NT hash known vectors");
 }
}
