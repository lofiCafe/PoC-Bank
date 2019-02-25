using InvertedTomato.IO;
using System;

namespace WinAce
{
    public class Utils
    {
        public static UInt32 CRC32(byte[] data)
        {
            var crc = CrcAlgorithm.CreateCrc32Jamcrc();
            crc.Append(data);
            return (UInt32)crc.ToUInt64();
        }

        public static UInt16 CRC16(byte[] data)
        {
            return (UInt16)((CRC32(data) & 0xFFFF));
        }
    }
}
