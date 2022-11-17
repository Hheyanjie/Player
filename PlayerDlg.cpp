
// PlayerDlg.cpp : implementation file
//

#include "pch.h"
#include "framework.h"
#include "Player.h"
#include "PlayerDlg.h"
#include "afxdialogex.h"
#include <iostream>
#include "Transcode.h"
using namespace std;
#ifdef _DEBUG
#define new DEBUG_NEW
#endif
#define SDL_REFRESH (SDL_USEREVENT + 1)

// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CPlayerDlg dialog

static bool b_pause = false;
static bool b_exit = false;
static bool b_needQuit = false;

CPlayerDlg::CPlayerDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_PLAYER_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CPlayerDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CPlayerDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDOK, &CPlayerDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDC_BUTTON1, &CPlayerDlg::OnBnClickedAudio)
	ON_BN_CLICKED(IDC_BUTTON2, &CPlayerDlg::OnBnClickedEncodeVideo)
END_MESSAGE_MAP()


// CPlayerDlg message handlers

BOOL CPlayerDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CPlayerDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CPlayerDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CPaintDC dc(this);
		if (m_hbitmap)
		{
			CRect rcClient;
			GetClientRect(rcClient);
			CDC memDc;
			memDc.CreateCompatibleDC(&dc);
			memDc.SelectObject(m_hbitmap);
			SetStretchBltMode(dc.m_hDC, HALFTONE);
			dc.StretchBlt(rcClient.left, rcClient.top, m_iWid, m_iHei, &memDc, 0, 0, m_iWid, m_iHei, SRCCOPY);
			memDc.DeleteDC();
		}
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CPlayerDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

DWORD WINAPI ShowVideoThread(LPVOID param)
{
	CPlayerDlg* pClass = (CPlayerDlg*)param;
	pClass->ShowWithSDL();
	//pClass->MainFun();
	return 0;
}

//回调函数的作用是填充音频缓冲区，当音频设备需要更多数据的时候会调用该回调函数
//userdata一般不使用，stream是音频缓冲区，len是音频缓冲区大小
void fill_audio(void* udata, Uint8* stream, int len) {
	//将stream置0，SDL2必须有的操作
	SDL_memset(stream, 0, len);
	
	if (audio_len == 0)		//如果没有剩余数据
		return;
	len = (len > audio_len ? audio_len : len);	//尽可能得到更多的数据
	//混音处理
	SDL_MixAudio(stream, audio_pos, len, SDL_MIX_MAXVOLUME);
	audio_pos += len;   //更新音频当前的位置
	audio_len -= len;   //更新音频剩下的长度
}

DWORD WINAPI Play_Audio(LPVOID param)
{
	CPlayerDlg* pClass = (CPlayerDlg*)param;
	//pClass->Play_Audio_Start();
	
	return 0;
}

int CPlayerDlg::Play_Audio_Start()
{
	const char* audio_path = "C:\\Users\\10296\\Desktop\\normal video.mp4";//视频路径

	//初始化ffmpeg组件
	AVFormatContext* pFormatCtx = nullptr;
	int i, audioStream = -1, videoStream = -1;
	AVCodecParameters* pCodecParameters = nullptr;
	AVCodecContext* pCodecCtx = nullptr;
	const AVCodec* pCodec = nullptr;
	AVFrame* pFrame = nullptr;
	AVPacket* packet;
	uint8_t* out_buffer;

	int64_t in_channel_layout;

	//初始化重采样组件
	struct SwrContext* au_convert_ctx;

	//打开音频文件 ffmpeg
	if (avformat_open_input(&pFormatCtx, m_strFileName, nullptr, nullptr) != 0) {
		cout << "can not open the audio!" << endl;
		return -1;
	}

	//寻找视频流与音频流 ffmpeg
	audioStream = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
	if (audioStream == -1) {
		cout << "can not find audio stream!" << endl;
		return -1;
	}
	videoStream = av_find_best_stream(pFormatCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
	if (videoStream == -1) {
		cout << "can not open a video stream" << endl;
		return -1;
	}

	//寻找解码器 ffmpeg
	pCodecParameters = pFormatCtx->streams[audioStream]->codecpar;
	pCodec = avcodec_find_decoder(pFormatCtx->streams[audioStream]->codecpar->codec_id);
	if (pCodec == nullptr) {
		cout << "can not find a codec" << endl;
		return -1;
	}

	//加载解码器参数 ffmpeg
	pCodecCtx = avcodec_alloc_context3(pCodec);
	if (avcodec_parameters_to_context(pCodecCtx, pCodecParameters) != 0) {
		cout << "can not copy codec context" << endl;
		return -1;
	}

	//启动解码器 ffmpeg
	if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
		cout << "can not open the decoder!" << endl;
		return -1;
	}

	//初始化packet与pframe ffmpeg
	packet = (AVPacket*)av_malloc(sizeof(AVPacket));
	av_packet_alloc();
	pFrame = av_frame_alloc();

	//音频参数初始化
	uint64_t out_channel_layout = AV_CH_LAYOUT_STEREO;//输出声道
	int out_nb_samples = 1024; //音频缓冲区的采样个数
	enum AVSampleFormat out_sample_fmt = AV_SAMPLE_FMT_S16;//输出格式S16
	int out_sample_rate = 44100; //pcm采样率
	int out_channels = av_get_channel_layout_nb_channels(out_channel_layout);//声道数量

	int out_buffer_size = av_samples_get_buffer_size(NULL, out_channels, out_nb_samples, out_sample_fmt, 1);//计算音频缓冲区大小
	out_buffer = (uint8_t*)av_malloc(MAX_AUDIO_FRAME_SIZE * 2);//初始化音频缓冲区大小

	//初始化SDL
	if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		cout << "can not open the SDL!" << endl;
		return -1;
	}

	//设置音频相关参数
	SDL_AudioSpec spec;
	spec.freq = out_sample_rate;  //pcm采样频率
	spec.format = AUDIO_S16SYS;   //音频格式，16bit
	spec.channels = out_channels; //声道数量
	spec.silence = 0;             //设置静音的值   
	spec.samples = out_nb_samples;//音频缓冲区的采样个数
	spec.callback = fill_audio;   //回调函数
	spec.userdata = pCodecCtx;    //回调函数的数据来自解码器

	//初始化SDL
	if (SDL_OpenAudio(&spec, nullptr) < 0) {
		cout << "can not open audio" << endl;
		return -1;
	}

	//音频重采样操作
	in_channel_layout = av_get_default_channel_layout(pCodecCtx->channels);
	cout << "in_channel_layout: " << in_channel_layout << endl;
	au_convert_ctx = swr_alloc(); //初始化重采样结构体
	au_convert_ctx = swr_alloc_set_opts(au_convert_ctx, out_channel_layout, out_sample_fmt, out_sample_rate, in_channel_layout, pCodecCtx->sample_fmt, pCodecCtx->sample_rate, 0, NULL);//重采样结构体赋值
	swr_init(au_convert_ctx);//将重采样结构体参数加载

	SDL_PauseAudio(0);

	//循环读取packet
	while (av_read_frame(pFormatCtx, packet) >= 0) {
		if (b_exit)
			goto QUIT;
		while (b_pause)
		{
			if (b_exit)
				goto QUIT;
		}
		if (packet->stream_index == audioStream) {
			//进行解码操作
			avcodec_send_packet(pCodecCtx, packet);
			while (avcodec_receive_frame(pCodecCtx, pFrame) == 0)
			{
				//将输入的音频按照定义的参数进行转换，并输出
				swr_convert(au_convert_ctx, &out_buffer, MAX_AUDIO_FRAME_SIZE, (const uint8_t**)pFrame->data, pFrame->nb_samples);
			}

			audio_chunk = (Uint8*)out_buffer; //设置音频缓冲区
			audio_len = out_buffer_size;      //更新音频长度
			audio_pos = audio_chunk;          //更新音频位置
			while (audio_len > 0)
			{
				SDL_Delay(1);//延迟播放
			}
		}
		av_packet_unref(packet);//清空packet内容
	}

QUIT:
	swr_free(&au_convert_ctx);//清空重采样结构体内容
	//回收ffmpeg组件
	if (pFrame) {
		av_frame_free(&pFrame);
		pFrame = nullptr;
	}

	if (pCodecCtx) {
		avcodec_close(pCodecCtx);
		pCodecCtx = nullptr;
		pCodec = nullptr;
	}

	if (pFormatCtx) {
		avformat_close_input(&pFormatCtx);
		pFormatCtx = nullptr;
	}
	av_free(out_buffer);

	//关闭SDL
	if(b_needQuit)
		SDL_Quit();
	b_needQuit = true;
	cout << "succeed!" << endl;
	return 0;
}

int sdl_refreshing(void* rate)
{
	SDL_Event event;
	int frame_rate = *((int*)rate);
	int interval = frame_rate > 0 ? 1000 / frame_rate : 33;
	while (!b_exit)
	{
		if (!b_pause)
		{
			event.type = SDL_REFRESH;
			SDL_PushEvent(&event);
			//Sleep(interval);
		}
		SDL_Delay(interval);
	}
	return 0;
}

int CPlayerDlg::ShowWithSDL()
{
	SDL_Window* screen;
	SDL_Renderer* sdl_renderer;
	SDL_Texture* sdl_texture;
	SDL_Rect            sdl_rect;


	AVFormatContext* pFormatCtx;
	int i, videoStream, screen_w, screen_h, PictureSize, ret, got_picture, frameFinished, frame_rate;
	AVCodecContext* pCodecCtx;
	AVCodec* pCodec;
	AVFrame* pFrame, * pFrameYUV;
	AVPacket* packet;
	struct SwsContext* pSwsCtx;

	FILE* fp_yuv;
	uint8_t* OutBuff;

	//char filepath[] = "C:\\Users\\10296\\Desktop\\normal video.mp4";
	//char filepath[] = "1.mp4";
	//char filepath[] = "test.wmv";
	avformat_network_init();
	pFormatCtx = avformat_alloc_context();
	//打开视频文件
	if (avformat_open_input(&pFormatCtx, m_strFileName.GetBuffer(), NULL, NULL) != 0) {
		MessageBox(L"cannot open file", L"错误", MB_OK);
		return -1;
	}
	//发现视频流上下文
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
		MessageBox(L"cannot find stream info\n", L"错误", MB_OK);
		return -1;
	}
	//在视频流群中找到类型为video的视频流
	videoStream = -1;
	for (i = 0; i < pFormatCtx->nb_streams; i++)
	{
		if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			frame_rate = pFormatCtx->streams[i]->avg_frame_rate.num / pFormatCtx->streams[i]->avg_frame_rate.den;
			break;
		}
	}
	if (videoStream == -1) {
		MessageBox(L"codec not found", L"错误", MB_OK);
		return -1;
	}
	//在video视频流中找到编解码器上下文
	pCodecCtx = avcodec_alloc_context3(NULL);
	//pCodecCtx = pFormatCtx->streams[videoStream]->codecpar;
	avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[videoStream]->codecpar);
	//根据编解码器上下文找到编解码器
	pCodec = (AVCodec*)avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL) {
		MessageBox(L"codec not found\n", L"错误", MB_OK);
		return -1;
	}
	//打开编解码器
	avcodec_open2(pCodecCtx, pCodec, NULL);
	int x = pCodec->id;
	pFrame = av_frame_alloc();
	pFrameYUV = av_frame_alloc();
	PictureSize = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1);
	OutBuff = (uint8_t*)av_malloc(PictureSize);
	//填充帧数据
	av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize,
		OutBuff, AV_PIX_FMT_YUV420P, pCodecCtx->width, pCodecCtx->height, 1);

	//设置图像转换上下文
	pSwsCtx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
		pCodecCtx->pix_fmt,
		pCodecCtx->width, pCodecCtx->height,
		AV_PIX_FMT_YUV420P,
		SWS_BICUBIC, NULL, NULL, NULL);
	m_iWid = pCodecCtx->width;
	m_iHei = pCodecCtx->height;
	int iRet = 0;

	//// B1. 初始化SDL子系统：缺省(事件处理、文件IO、线程)、视频、音频、定时器
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
	{
		printf("SDL_Init() failed: %s\n", SDL_GetError());
		return -1;
	}

	// B2. 创建SDL窗口，SDL 2.0支持多窗口
	//     SDL_Window即运行程序后弹出的视频窗口，同SDL 1.x中的SDL_Surface
	screen = SDL_CreateWindow("Simplest ffmpeg player's Window",
		SDL_WINDOWPOS_UNDEFINED,// 不关心窗口X坐标
		SDL_WINDOWPOS_UNDEFINED,// 不关心窗口Y坐标
		pCodecCtx->width,
		pCodecCtx->height,
		SDL_WINDOW_OPENGL
	);

	if (screen == NULL)
	{
		printf("SDL_CreateWindow() failed: %s\n", SDL_GetError());
		return -1;
	}

	// B3. 创建SDL_Renderer
	//     SDL_Renderer：渲染器
	sdl_renderer = SDL_CreateRenderer(screen, -1, 0);

	// B4. 创建SDL_Texture
	//     一个SDL_Texture对应一帧YUV数据，同SDL 1.x中的SDL_Overlay
	//     此处第2个参数使用的是SDL中的像素格式，对比参考注释A7
	//     FFmpeg中的像素格式AV_PIX_FMT_YUV420P对应SDL中的像素格式SDL_PIXELFORMAT_IYUV
	sdl_texture = SDL_CreateTexture(sdl_renderer,
		SDL_PIXELFORMAT_IYUV,
		SDL_TEXTUREACCESS_STREAMING,
		pCodecCtx->width,
		pCodecCtx->height);

	sdl_rect.x = 0;
	sdl_rect.y = 0;
	sdl_rect.w = pCodecCtx->width;
	sdl_rect.h = pCodecCtx->height;


	int quit = 0;
	SDL_Event event;

	packet = (AVPacket*)av_malloc(sizeof(AVPacket));
	SDL_Thread* sdl_thread = SDL_CreateThread(sdl_refreshing, NULL, (void*)&frame_rate);

	//读入一个包
	while (1)
	{
		SDL_WaitEvent(&event);
		if (event.type == SDL_REFRESH)
		{
			while (av_read_frame(pFormatCtx, packet) == 0) {
				if (packet->stream_index == videoStream) {
					//解码
					iRet = avcodec_send_packet(pCodecCtx, packet);

					if ((iRet = avcodec_receive_frame(pCodecCtx, pFrame)) == 0)
					{
						sws_scale(pSwsCtx,                                  // sws context
							(const uint8_t* const*)pFrame->data,  // src slice
							pFrame->linesize,                      // src stride
							0,                                        // src slice y
							pCodecCtx->height,                      // src slice height
							pFrameYUV->data,                          // dst planes
							pFrameYUV->linesize                       // dst strides
						);

						// B5. 使用新的YUV像素数据更新SDL_Rect
						SDL_UpdateYUVTexture(sdl_texture,                   // sdl texture
							&sdl_rect,                     // sdl rect
							pFrameYUV->data[0],            // y plane
							pFrameYUV->linesize[0],        // y pitch
							pFrameYUV->data[1],            // u plane
							pFrameYUV->linesize[1],        // u pitch
							pFrameYUV->data[2],            // v plane
							pFrameYUV->linesize[2]         // v pitch
						);

						// B6. 使用特定颜色清空当前渲染目标
						SDL_RenderClear(sdl_renderer);
						// B7. 使用部分图像数据(texture)更新当前渲染目标
						SDL_RenderCopy(sdl_renderer,                        // sdl renderer
							sdl_texture,                         // sdl texture
							NULL,                                // src rect, if NULL copy texture
							&sdl_rect                            // dst rect
						);
						// B8. 执行渲染，更新屏幕显示
						SDL_RenderPresent(sdl_renderer);
						av_packet_unref(packet);
						// B9. 控制帧率为25FPS，此处不够准确，未考虑解码消耗的时间
						//SDL_Delay(40);
						//SDL_Delay(33);
						break;
					}
					break;
				}
			}
		}
		else if (event.type == SDL_QUIT || event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
		{
			b_exit = true;
			goto QUIT;
		}
		else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE)
		{
			b_pause = !b_pause;
		}
		
	}

	
QUIT:
	if(b_needQuit)
		SDL_Quit();
	b_needQuit = true;
	//av_packet_unref(packet);
	sws_freeContext(pSwsCtx);
	av_free(OutBuff);
	av_free(pFrame);
	av_free(pFrameYUV);
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);

	return 0;
}

void CPlayerDlg::OnBnClickedOk()
{
	CString FilePathName;
	CString Filter = CString("视频文件|*.mp4;*.mpeg;*.avi;*.mkv;*.rmvb;*.wmv;*.flv||");
	CString fileName = L"";
	CFileDialog dlg(TRUE, NULL, fileName, OFN_HIDEREADONLY | OFN_READONLY, Filter, NULL);
	//dlg.m_ofn.lpstrInitialDir = L"C:\\Users\\10296\\Desktop\\";
	if (dlg.DoModal() != IDOK)
		return;

	m_strFileName = dlg.GetPathName();
	//m_FileUrlEdit.SetWindowText(FilePathName);
	//CStringA FilePath;
	//GetWindowTextA(m_FileUrlEdit.m_hWnd, (LPSTR)(LPCSTR)FilePath, 200);
	//m_ScreenArea.SetFile(FilePath);
	//m_StartButton.EnableWindow(TRUE);
	b_exit = false;
	b_pause = false;
	b_needQuit = false;
	// TODO: Add your control notification handler code here
	DWORD dwId1,dwId2;
	CreateThread(NULL, NULL, ShowVideoThread, (LPVOID)this, NULL, &dwId1);
	CreateThread(NULL, NULL, Play_Audio, (LPVOID)this, NULL, &dwId2);
	//CDialogEx::OnOK();
}

int CPlayerDlg::MainFun() {
	SDL_Window* screen;
	SDL_Renderer* sdl_renderer;
	SDL_Texture* sdl_texture;
	SDL_Rect            sdl_rect;


	AVFormatContext* pFormatCtx;
	int i, videoStream, screen_w, screen_h, PictureSize, ret, got_picture, frameFinished;
	AVCodecContext* pCodecCtx;
	AVCodec* pCodec;
	AVFrame* pFrame, * pFrameRGB;
	AVPacket* packet;
	struct SwsContext* pSwsCtx;

	FILE* fp_yuv;
	uint8_t* OutBuff;

	char filepath[] = "C:\\Users\\10296\\Desktop\\Sample_360.mp4";
	//char filepath[] = "1.mp4";
	//char filepath[] = "test.wmv";
	avformat_network_init();
	pFormatCtx = avformat_alloc_context();
	//打开视频文件
	if (avformat_open_input(&pFormatCtx, filepath, NULL, NULL) != 0) {
		MessageBox(L"cannot open file", L"错误", MB_OK);
		return -1;
	}
	//发现视频流上下文
	if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
		MessageBox(L"cannot find stream info\n", L"错误", MB_OK);
		return -1;
	}
	//在视频流群中找到类型为video的视频流
	videoStream = -1;
	for (i = 0; i < pFormatCtx->nb_streams; i++)
	{
		if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			videoStream = i;
			break;
		}
	}
	if (videoStream == -1) {
		MessageBox(L"codec not found", L"错误", MB_OK);
		return -1;
	}
	//在video视频流中找到编解码器上下文
	pCodecCtx = avcodec_alloc_context3(NULL);
	//pCodecCtx = pFormatCtx->streams[videoStream]->codecpar;
	avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[videoStream]->codecpar);
	//根据编解码器上下文找到编解码器
	pCodec = (AVCodec*)avcodec_find_decoder(pCodecCtx->codec_id);
	if (pCodec == NULL) {
		MessageBox(L"codec not found\n", L"错误", MB_OK);
		return -1;
	}
	//打开编解码器
	avcodec_open2(pCodecCtx, pCodec, NULL);
	pFrame = av_frame_alloc();
	pFrameRGB = av_frame_alloc();
	PictureSize = av_image_get_buffer_size(AV_PIX_FMT_BGR24, pCodecCtx->width, pCodecCtx->height, 1);
	OutBuff = (uint8_t*)av_malloc(PictureSize);
	//填充帧数据
	av_image_fill_arrays(pFrameRGB->data, pFrameRGB->linesize,
		OutBuff, AV_PIX_FMT_BGR24, pCodecCtx->width, pCodecCtx->height, 1);

	//设置图像转换上下文
	pSwsCtx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
		pCodecCtx->pix_fmt,
		pCodecCtx->width, pCodecCtx->height,
		AV_PIX_FMT_BGR24,
		SWS_BICUBIC, NULL, NULL, NULL);
	m_iWid = pCodecCtx->width;
	m_iHei = pCodecCtx->height;
	int iRet = 0;

	packet = (AVPacket*)av_malloc(sizeof(AVPacket));
	//读入一个包
	while (av_read_frame(pFormatCtx, packet) >= 0) {
		if (packet->stream_index == videoStream) {
			//解码
			iRet = avcodec_send_packet(pCodecCtx, packet);
			
			while ((iRet = avcodec_receive_frame(pCodecCtx, pFrame)) == 0)
			{
				sws_scale(pSwsCtx,
					(const uint8_t* const*)pFrame->data,
					pFrame->linesize,
					0, pCodecCtx->height,
					pFrameRGB->data, pFrameRGB->linesize);

				ShowInDlg(pFrameRGB, pCodecCtx->width, pCodecCtx->height, i++, 24);
				Sleep(33);
				av_frame_unref(pFrame);
			}
		}
	}
	sws_freeContext(pSwsCtx);
	av_free(pFrame);
	av_free(pFrameRGB);
	avcodec_close(pCodecCtx);
	avformat_close_input(&pFormatCtx);
	return 0;
}

int CPlayerDlg::ShowInDlg(AVFrame* pFrameRGB, int width, int height, int index, int bpp) {
	char buf[5] = { 0 };
	BITMAPFILEHEADER bmpheader;
	BITMAPINFOHEADER bmpinfo;
	FILE* fp;
	bmpheader.bfType = 0x4d42;
	bmpheader.bfReserved1 = 0;
	bmpheader.bfReserved2 = 0;
	bmpheader.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
	bmpheader.bfSize = bmpheader.bfOffBits + width * height * bpp / 8;

	bmpinfo.biSize = sizeof(BITMAPINFOHEADER);
	bmpinfo.biWidth = width;
	bmpinfo.biHeight = -height;
	bmpinfo.biPlanes = 1;
	bmpinfo.biBitCount = bpp;
	bmpinfo.biCompression = BI_RGB;
	bmpinfo.biSizeImage = (width * bpp + 31) / 32 * 4 * height;
	bmpinfo.biXPelsPerMeter = 0;
	bmpinfo.biYPelsPerMeter = 0;
	bmpinfo.biClrUsed = 0;
	bmpinfo.biClrImportant = 0;
	if (m_hbitmap)
		::DeleteObject(m_hbitmap);
	CClientDC dc(NULL);
	m_hbitmap = CreateDIBitmap(dc.GetSafeHdc(),							//设备上下文的句柄 
		(LPBITMAPINFOHEADER)&bmpinfo,				//位图信息头指针 
		(long)CBM_INIT,								//初始化标志 
		pFrameRGB->data[0],						//初始化数据指针 
		(LPBITMAPINFO)&bmpinfo,						//位图信息指针 
		DIB_RGB_COLORS);

	static int x = 1;
	if (x <= 10)
	{
		BITMAP bm;
		CBitmap::FromHandle(m_hbitmap)->GetBitmap(&bm);
		int nbyte = bm.bmBitsPixel / 8;
		BITMAPINFO bi;
		ZeroMemory(&bi, sizeof(BITMAPINFO));
		BYTE* pBits = (BYTE*)new BYTE[bm.bmWidth * bm.bmHeight * nbyte];
		::ZeroMemory(pBits, bm.bmWidth * bm.bmHeight * nbyte);

		if (!::GetDIBits(NULL, m_hbitmap, 0, bm.bmHeight, pBits, &bi, DIB_RGB_COLORS))
		{
			delete pBits;
			pBits = NULL;
			//EndCommand();
			//return cxOK;
		}

		CImage Image;
		Image.CreateEx(bm.bmWidth, -bm.bmHeight, bm.bmBitsPixel, BI_RGB, NULL, 0x000000FF);

		int nPitch = Image.GetPitch(), nBPP = Image.GetBPP();
		LPBYTE lpBits = reinterpret_cast<LPBYTE>(Image.GetBits());
		uint8_t  R, G, B;
		int pp = 0;
		for (int i = 0; i < bm.bmHeight; i++)
		{
			LPBYTE lpBytes = lpBits + (i * nPitch);
			byte* toByte = lpBytes;
			for (int j = 0; j < bm.bmWidth; j++)
			{
				B = pFrameRGB->data[0][3 * (bm.bmWidth * i + j) + 0];
				G = pFrameRGB->data[0][3 * (bm.bmWidth * i + j) + 1];
				R = pFrameRGB->data[0][3 * (bm.bmWidth * i + j) + 2];
				lpBits[pp++] = B;
				lpBits[pp++] = G;
				lpBits[pp++] = R;
				lpBits[pp++] = 255;
			}
		}
		int xx = pp;
		wchar_t ss[100];
		swprintf_s(ss, L"C:\\Users\\10296\\Desktop\\ss\\%d.png", x);
		Image.Save(ss, Gdiplus::ImageFormatPNG);
		Image.Detach();
	}
	x++;

	CRect rt(0, 0, width, height);
	InvalidateRect(&rt, FALSE);
	//Invalidate();
	return 0;
}


void CPlayerDlg::OnBnClickedAudio()
{
	// TODO: Add your control notification handler code here
	DWORD dwId;
	CreateThread(NULL, NULL, Play_Audio, (LPVOID)this, NULL, &dwId);
}


void CPlayerDlg::OnBnClickedEncodeVideo()
{
	// TODO: Add your control notification handler code here
	CString FilePathName;
	CString Filter = CString("视频文件|*.mp4;*.mpeg;*.avi;*.mkv;*.rmvb;*.wmv;*.flv||");
	CString fileName = L"";
	CFileDialog dlg(TRUE, NULL, fileName, OFN_HIDEREADONLY | OFN_READONLY, Filter, NULL);
	//dlg.m_ofn.lpstrInitialDir = L"C:\\Users\\10296\\Desktop\\";
	if (dlg.DoModal() != IDOK)
		return;
	m_strFileName = dlg.GetPathName();

	Transcode* transcode = new Transcode(m_strFileName.GetBuffer(), "C:\\Users\\10296\\Desktop\\out\\out.mp4");
	transcode->doTranscode();
	MessageBox(L"succeed", L"succeed");
}
