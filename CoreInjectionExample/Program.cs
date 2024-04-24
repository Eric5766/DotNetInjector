using System.Runtime.InteropServices;

namespace CoreInjectionExample
{
    internal class Program
    {
        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        static extern bool AllocConsole();
        static void Main(string[] args)
        {
            EntryPoint(null);
        }

        static int EntryPoint(String pwzArgument)
        {
            AllocConsole();
            Console.WriteLine("I made it in! I'm running inside: [" + System.Diagnostics.Process.GetCurrentProcess().ProcessName + "]");
            Console.WriteLine("The .NET Framework I'm using is: [" + RuntimeInformation.FrameworkDescription + "]");
            Console.ReadLine();
            return 0;
        }
    }
}
