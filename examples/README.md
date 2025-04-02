

// apt-get install -y ffmpeg
// ffmpeg -i Big_Buck_Bunny_1080_10s_1MB.mp4 -vf scale=1920:1080 -vframes 100 -c:v rawvideo -pix_fmt yuv420p sample.yuv









media::XVideoDeviceConfig config {
	.width = 1920,
	.height = 1080,
	.framerate = 60,
        .cursor = false,
        display_id = ":0",
};

auto display = media::XVideoDevice::Create(config);
if(!encoder) { 

}


std::vector<uint8_t> yuv_data;

int success = display->GetFrameYUV420(&yuv_data);
if(!success) {

}


int success = display->GetFrameNV12(&yuv_data);
if(!success) {

}

create the files below like above using libs like libxcb, avcodec for auto convert to nv12 if gpu present else 


// these files have a function called FrameYUV420
image_utils.cc
image_utils.h


after process is compiled with cmake vs studio it exits instantly not even logging or prints so we need to debug what deps or dlls it's missing



"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\14.41.34120\bin\Hostx64\x64\dumpbin.exe"/DEPENDENTS "C:\Users\\Cloud\\Downloads\\libmediacodec-main\\build\\examples\\Debug\\vp8_encoder.exe"


"C:\Program Files (x86)\Microsoft Visual Studio\[version]\[edition]\VC\Tools\MSVC\[version]\bin\Hostx64\x64\dumpbin.exe" /DEPENDENTS your_file.exe



Get-Process | Where-Object { $_.Path -eq "C:\Users\Cloud\Downloads\libmediacodec-main\build\examples\Debug\exe" } | Debug-Process

# Replace "program.exe" with the name of your program


(Get-Process -Name "program").Modules | Select-Object FileName

dumpbin /dependents program.exe


auto utils = media::ImageUtils();
if(!utils) { 

}

utils->setGPU(true); <= for nv12 fast convert nvidia
else use software 


std::vector<uint8_t> nv12_data;
std::vector<uint8_t> data; <= this could be type rgb, rgba, brga, nv12, yuv420, so you have to auto detect and convert

utils->ConvertToNV12(data, nv12_data);

std::vector<uint8_t> yuv_420;
utils->ConvertToYUV420(data, yuv_420);


create the files below like above using libs like avcodec for auto convert to nv12 if gpu flat nt else 

image_utils.cc
image_utils.h


$ wget http://test-videos.co.uk/vids/bigbuckbunny/mp4/h264/1080/Big_Buck_Bunny_1080_10s_1MB.mp4





$ time ffmpeg -y -i output_v2.mp4 -vf "format=nv12,scale=1280:720" -c:a copy -c:v h264_nvenc out1.mp4

real    0m2.899s
user    0m4.407s
sys     0m0.439s

VS

$ time ffmpeg -y -hwaccel cuda -i output_v2.mp4 -vf "hwupload_cuda,scale_cuda=1280:720,hwdownload,format=nv12" -c:a copy -c:v h264_nvenc out2.mp4

real    0m2.810s
user    0m1.055s
sys     0m0.844s



















 