using System.Net;
using System.Net.Sockets;
using System.Buffers.Binary;
using ViceTasks.Ultimate;
using SixLabors.ImageSharp;
using SixLabors.ImageSharp.PixelFormats;
using System.Text.Json;
var output = "/Users/karlprosserpp/dev/c64projects/gdos64-bridge/evidence/ultimate";
using var client = new UltimateClient(new UltimateClientOptions {Host="10.0.0.79"});
using var udp=new UdpClient(12065);
udp.Client.ReceiveBufferSize=8*1024*1024;
await client.StartStreamAsync("video",IPAddress.Parse("10.0.0.15"),12065);
Console.WriteLine($"PID {Environment.ProcessId}: passive VIC capture started");
int[] pal={0x000000,0xefefef,0x8d2f34,0x6ad4cd,0x9835a4,0x4cb442,0x2c29b1,0xefef5d,0x984e20,0x5b3800,0xd1676d,0x4a4a4a,0x7b7b7b,0x9fef93,0x6d6aef,0xb2b2b2};
long count=0;int current=-1;var rows=new Dictionary<int,byte[]>();var last=DateTime.MinValue;
while(true) {
 var d=(await udp.ReceiveAsync()).Buffer;
 if(!File.Exists(output+"/packet.json"))File.WriteAllText(output+"/packet.json",JsonSerializer.Serialize(new {length=d.Length,header=Convert.ToHexString(d[..12])}));
 if(d.Length<12)continue;
 int frame=BinaryPrimitives.ReadUInt16LittleEndian(d.AsSpan(2));
 int lineWord=BinaryPrimitives.ReadUInt16LittleEndian(d.AsSpan(4));
 int width=BinaryPrimitives.ReadUInt16LittleEndian(d.AsSpan(6));
 int n=d[8],bits=d[9],encoding=BinaryPrimitives.ReadUInt16LittleEndian(d.AsSpan(10));
 if(width!=384 || n!=4 || bits!=4 || encoding!=0)continue;
 if(frame!=current){rows.Clear();current=frame;}
 int line=lineWord&32767; rows[line]=d[12..];
 if((lineWord&32768)==0 || (DateTime.Now-last).TotalMilliseconds<300)continue;
 int height=line+n;
 if(Enumerable.Range(0,height/4).Any(i=>!rows.ContainsKey(i*4)))continue;
 using var im=new Image<Rgb24>(width,height);
 for(int y=0;y<height;y++)for(int x=0;x<width;x++){
  byte packed=rows[(y/4)*4][(y%4)*width/2+x/2];
  int color=pal[x%2==0?packed&15:packed>>4];im[x,y]=new Rgb24((byte)(color>>16),(byte)(color>>8),(byte)color);
 }
 await im.SaveAsPngAsync(output+"/latest.dotnet.png");File.Move(output+"/latest.dotnet.png",output+"/latest.png",true);
 File.WriteAllText(output+"/stream.json",JsonSerializer.Serialize(new {frames=++count,time=DateTimeOffset.UtcNow,size=new[]{width,height}}));last=DateTime.Now;
}
