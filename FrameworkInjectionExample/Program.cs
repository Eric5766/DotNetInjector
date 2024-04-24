using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;


namespace FrameworkInjectionExample
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
