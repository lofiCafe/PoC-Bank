using System;
using System.IO;
using System.Linq;

namespace WinAce
{
    public class AceFile
    {
        private UInt32 packsize;        // uint32|64 packed size
        private UInt32 origsize;        // uint32|64 original size
        private UInt32 datetime;        // uint32    ctime
        private UInt32 attribs;         // uint32    file attributes
        private UInt32 crc32;           // uint32    checksum over compressed file
        private byte comptype;          // uint8     compression type
        private byte compqual;          // uint8     compression quality
        private UInt16 parameters;      // uint16    decompression parameters
        private UInt16 reserved1;       // uint16
        private byte[] filename;        // [uint16]
        private UInt16[] comment;         // [uint16]  optional, compressed
        private UInt16[] ntsecurity;      // [uint16]  optional
        private byte[] reserved2;       // [?]

        private UInt16 crc;
        private UInt16 size;
        private byte type;
        private UInt16 flags;

        public string FileName { get; private set; }
        public string ExtractPath { get; private set; }
        public string Comment { get; private set; }

        public byte[] Headers { get; private set; }

        public AceFile(string fileName, string extractPath, string comment = null)
        {
            this.FileName = fileName;
            this.ExtractPath = extractPath;
            this.Comment = comment;

            this.type = 0x01;                   // FILE32
            this.flags = 0x8001;                // ADDSIZE|SOLID

            this.packsize = 4;
            this.origsize = 4;
            this.datetime = 0x4e5554fc;
            this.attribs = 0x00000020;          // ARCHIVE            
            this.comptype = 0x00;               // stored
            this.compqual = 0x03;               // normal
            this.parameters = 0x000a;
            this.reserved1 = 0x9e20;
            this.filename = this.ExtractPath.ToCharArray().Select(c => (byte)c).ToArray();
            this.comment = (this.Comment != null) ? this.Comment.ToCharArray().Select(c => (UInt16)c).ToArray() : new UInt16[] { };
            this.ntsecurity = new UInt16[] { };
            this.reserved2 = new byte[] { };
        }

        public void SetDatetime(UInt32 datetime)
        {
            this.datetime = datetime;
        }

        private void GetHeadersSize()
        {
            int size = 0;

            size += sizeof(byte);                         // uint16            - type
            size += sizeof(UInt16);                         // uint16           - flags

            size += sizeof(UInt32);                         // uint32           - packsize
            size += sizeof(UInt32);                         // uint32           - origsize
            size += sizeof(UInt32);                         // uint32           - datetime
            size += sizeof(UInt32);                         // uint32           - attribs
            size += sizeof(UInt32);                         // uint32           - crc32
            size += sizeof(byte);                           // uint8            - comptype
            size += sizeof(byte);                           // uint8            - compqual
            size += sizeof(UInt16);                         // uint16           - parameters
            size += sizeof(UInt16);                         // uint16           - reserved1
            size += sizeof(UInt16);                         // uint16           - filename length
            size += sizeof(byte) * this.filename.Length;    // uint8[]         - filename
            size += sizeof(UInt16) * this.comment.Length;   // uint16[]         - comment
            size += sizeof(UInt16) * this.ntsecurity.Length;// uint16[]         - ntsecurity
            size += sizeof(byte) * this.reserved2.Length;   // uint8[]          - reserved2

            this.size = (UInt16)size;
        }

        private void GetCRC()
        {
            this.crc = Utils.CRC16(this.Headers);

            int n = sizeof(UInt16) /* crc */ + sizeof(UInt16) /* size */;
            byte[] newHeaders = new byte[this.size + n];

            using (MemoryStream memStream = new MemoryStream())
            {
                using (BinaryWriter binStream = new BinaryWriter(memStream))
                {
                    binStream.Write(this.crc);
                    binStream.Write(this.size);
                    binStream.Write(this.Headers);

                    binStream.Flush();
                }

                this.Headers = new byte[newHeaders.Length];
                Buffer.BlockCopy(memStream.GetBuffer(), 0, this.Headers, 0, newHeaders.Length);
            }
        }

        public void GetHeaders()
        {
            using (MemoryStream memStream = new MemoryStream())
            {
                using (BinaryWriter binStream = new BinaryWriter(memStream))
                {
                    binStream.Write(this.type);
                    binStream.Write(this.flags);

                    binStream.Write(this.packsize);
                    binStream.Write(this.origsize);
                    binStream.Write(this.datetime);
                    binStream.Write(this.attribs);
                    binStream.Write(this.crc32);
                    binStream.Write(this.comptype);
                    binStream.Write(this.compqual);
                    binStream.Write(this.parameters);
                    binStream.Write(this.reserved1);
                    binStream.Write((UInt16)this.filename.Length);
                    binStream.Write(this.filename);
                    binStream.Write(this.comment.SelectMany(BitConverter.GetBytes).ToArray());
                    binStream.Write(this.ntsecurity.SelectMany(BitConverter.GetBytes).ToArray());
                    binStream.Write(this.reserved2);

                    binStream.Flush();
                }


                this.Headers = new byte[this.size];
                Buffer.BlockCopy(memStream.GetBuffer(), 0, this.Headers, 0, this.size);
            }
        }

        public bool Get()
        {
            if (!File.Exists(this.FileName))
            {
                return false;
            }

            byte[] fileData = File.ReadAllBytes(this.FileName);
            if (fileData == null || fileData.Length <= 0)
            {
                return false;
            }
            this.packsize = (UInt32) fileData.Length;
            this.origsize = (UInt32) fileData.Length;

            this.crc32 = Utils.CRC32(fileData);

            this.GetHeadersSize();
            this.GetHeaders();
            this.GetCRC();

            return true;
        }
    }
}
