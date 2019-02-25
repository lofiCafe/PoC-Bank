using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace WinAce
{
    public class AceVolume
    {
        private byte[] magic;       // uint8[7]  **ACE**                            7 * 1 = 7 bytes
        private byte eversion;      // uint8     extract version                    1 byte
        private byte cversion;      // uint8     creator version                    1 byte
        private byte host;          // uint8     platform                           1 byte
        private byte volume;        // uint8     volume number                      1 byte
        private UInt32 datetime;    // uint32    date/time in MS-DOS format         4 bytes
        private byte[] reserved1;   // uint8[8]                                     8 * 1 = 8 bytes
        private byte[] advert;      // [uint8]   optional                           1 * n bytes
        private UInt16[] comment;   // [uint16]  optional, compressed               2 bytes
        private byte[] reserved2;   // [?]       optional                           1 * n bytes

        private UInt16 crc;
        private UInt16 size;
        private byte type;
        private UInt16 flags;

        public byte[] Headers { get; private set; }
        public List<AceFile> Files = new List<AceFile>();

        public AceVolume()
        {
            this.type = 0x00;       // MAIN
            this.flags = 0x8100;    // V20FORMAT|SOLID

            this.magic = new byte[7]{0x2A, 0x2A, 0x41, 0x43, 0x45, 0x2A, 0x2A};//"**ACE**".ToCharArray().Select(c => (byte)c).ToArray();
            this.eversion = 20;
            this.cversion = 20;
            this.host = 0x02;
            this.volume = 0;
            this.datetime = 0x4e556ccf;
            this.reserved1 = new byte[8] { 0x30, 0x93, 0x66, 0x76, 0x4e, 0x20, 0x00, 0x00 };
            this.advert = new byte[] { };
            this.comment = new UInt16[] { };
            this.reserved2 = new byte[] { 0x00, 0x00, 0x00, 0x48, 0x35, 0x55, 0x03, 0x5D, 0x36, 0x20, 0x9E, 0x10, 0xFD, 0xC9, 0xFB, 0x45, 0x72, 0x73 }; // \x00\x00\x00H5U\x03]6 \x9e\x10\xfd\xc9\xfbErs
        }
 
        byte[] rawData = {
    
};

        public void AddFile(AceFile file)
        {
            this.Files.Add(file);
        }

        private void GetHeadersSize()
        {
            int size = 0;

            size += sizeof(byte);                           // uint8            - type
            size += sizeof(UInt16);                         // uint16           - flags

            size += sizeof(byte) * this.magic.Length;       // uint8 * length   - magic
            size += sizeof(byte);                           // uint8            - eversion
            size += sizeof(byte);                           // uint8            - cversion
            size += sizeof(byte);                           // uint8            - host
            size += sizeof(byte);                           // uint8            - volume
            size += sizeof(UInt32);                         // uint32           - datetime
            size += sizeof(byte) * this.reserved1.Length;   // uint8 * length   - reserved1
            size += sizeof(byte) * this.advert.Length;      // uint8 * length   - advert
            size += sizeof(UInt16) * this.comment.Length;   // uint16 * length  - comment
            size += sizeof(byte) * this.reserved2.Length;   // uint8 * length   - reserved2

            this.size = (UInt16)size;
        }

        private void GetCRC()
        {

            using (MemoryStream memStream = new MemoryStream())
            {
                using (BinaryWriter binStream = new BinaryWriter(memStream))
                {

                    binStream.Write(this.type);
                    binStream.Write(this.flags);

                    binStream.Write(this.magic);
                    binStream.Write(this.eversion);
                    binStream.Write(this.cversion);
                    binStream.Write(this.host);
                    binStream.Write(this.volume);
                    binStream.Write(this.datetime);
                    binStream.Write(this.reserved1);
                    binStream.Write(this.advert);
                    binStream.Write(this.comment.SelectMany(BitConverter.GetBytes).ToArray());
                    binStream.Write(this.reserved2);

                    binStream.Flush();                    
                }

                this.Headers = new byte[this.size];
                Buffer.BlockCopy(memStream.GetBuffer(), 0, this.Headers, 0, this.size);

                this.crc = Utils.CRC16(this.Headers);
            }
        }

        public void Save(string filename)
        {
            this.GetHeadersSize();
            this.GetCRC();

            using (FileStream fileStream = new FileStream(filename, FileMode.Create))
            {
                using (BinaryWriter binStream = new BinaryWriter(fileStream))
                {
                    binStream.Write(this.crc);
                    binStream.Write(this.size);
                    binStream.Write(this.type);
                    binStream.Write(this.flags);

                    binStream.Write(this.magic);
                    binStream.Write(this.eversion);
                    binStream.Write(this.cversion);
                    binStream.Write(this.host);
                    binStream.Write(this.volume);
                    binStream.Write(this.datetime);
                    binStream.Write(this.reserved1);
                    binStream.Write(this.advert);
                    binStream.Write(this.comment.SelectMany(BitConverter.GetBytes).ToArray());
                    binStream.Write(this.reserved2);

                    foreach (AceFile f in this.Files)
                    {
                        if (f.Get() && f.Headers != null && f.Headers.Length > 0)
                        {
                            binStream.Write(f.Headers);

                            byte[] fileData = File.ReadAllBytes(f.FileName);
                            if (fileData != null && fileData.Length > 0)
                            {
                                binStream.Write(fileData);
                            }
                        }
                    }

                    binStream.Flush();
                }
            }                        
        }
    }
}
