using ViceTasks.Ultimate;
using SixLabors.ImageSharp;
using var client = new UltimateClient(new UltimateClientOptions { Host = Environment.GetEnvironmentVariable("C64U_HOST") ?? "10.0.0.79" });
using var frame = await client.CaptureVideoFrameAsync(port:11004, timeout:TimeSpan.FromSeconds(8));
await frame.SaveAsPngAsync(args[0]);
