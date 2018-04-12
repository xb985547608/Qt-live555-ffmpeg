/**********
  This library is free software; you can redistribute it and/or modify it under
  the terms of the GNU Lesser General Public License as published by the
  Free Software Foundation; either version 2.1 of the License, or (at your
  option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

  This library is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
  more details.

  You should have received a copy of the GNU Lesser General Public License
  along with this library; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 **********/
// "liveMedia"
// Copyright (c) 2010 UPTTOP.COM, All rights reserved.
// Demultiplexer for AVI(Audio Video Inter) file
// Implementation

#include "AVIDemux.hh"
#include "AVIDemuxedElementaryStream.hh"
#include "StreamParser.hh"
#include "InputFile.hh"
#include "ByteStreamFileSource.hh"
#include "GroupsockHelper.hh"
#include <stdlib.h>

#define __UINT64_C(c)	c ## UL
#define INT64_MIN		(-__INT64_C(9223372036854775807)-1)
#define INT64_MAX		(__INT64_C(9223372036854775807))

//#define DEMUX_DEBUG

////////// AVIDemux::OutputDescriptor::SavedData definition/implementation //////////

class AVIDemux::OutputDescriptor::SavedData {
public:
	SavedData(unsigned char* buf, unsigned size, int64_t t)
		: next(NULL), data(buf), dataSize(size), pts(t){
	}
	virtual ~SavedData() {
		if(dataSize > 0)
			delete[] data;
		if(next != NULL)
			delete next;
	}

	SavedData* next;
	unsigned char* data;
	unsigned dataSize;
	int64_t  pts;
};

////////// AVIDemux implementation //////////

AVIDemux
::AVIDemux(UsageEnvironment& env,
		   char const* inputFileName)
		   : Medium(env) {
   	fInputFileName = strDup(inputFileName);
	
	for (unsigned i = 0; i < MAX_AVI_STREAM; ++i) {
		memset(&fOutput[i], 0, sizeof(OutputDescriptor_t));
		fOutput[i].isPotentiallyReadable = False;
		fOutput[i].isCurrentlyActive = False;
		fOutput[i].isCurrentlyAwaitingData = False;
	}

	fVideoIndex = -1;
	fAudioIndex = -1;
	av_register_all();
	fFormatCtx = NULL;
	fMicroSecPerFrame = 0;

	vop_time_increment_resolution = 0;
	fixed_vop_rate = 0;
	fixed_vop_time_increment = 0;
	fLast_fixed_pts= 0;
	fLast_vop_time_increment = 0;
	fLast_vop_coding_type = 0;
	fFirst_vop = True;

	fVideo_diff = 0.0;
	fVOPHeaderParsed = False;

	printf("#AVIDemux#0x%x AVIDemux() fVideoIndex = %d, fAudioIndex = %d\n", this, fVideoIndex, fAudioIndex);
}

AVIDemux::~AVIDemux()
{
	if (fInputFileName) 
		delete[] fInputFileName;

	for (unsigned i = 0; i < MAX_AVI_STREAM; ++i)
		delete fOutput[i].savedDataHead;

	// Close the video file
	if (fFormatCtx)
		av_close_input_file(fFormatCtx);
}

AVIDemux* AVIDemux
::createNew(UsageEnvironment& env,
		char const* inputFileName) 
{
	return new AVIDemux(env, inputFileName);
}


void AVIDemux::flushInput() {
	//  TBD -- should add some codes for SEEK 
}

double AVIDemux::convertPTS(AVPacket *pkt)
{
	double pts;
	if (pkt->pts != AV_NOPTS_VALUE) {
		pts = (double) (pkt->pts * fFormatCtx->streams[pkt->stream_index]->time_base.num) / fFormatCtx->streams[pkt->stream_index]->time_base.den;
	}
	else {
		pts = (double) (pkt->dts * fFormatCtx->streams[pkt->stream_index]->time_base.num) / fFormatCtx->streams[pkt->stream_index]->time_base.den;
	}
	return pts;
}

int AVIDemux::prepare()
{
	if(fFormatCtx != NULL && fVideoIndex != -1 && fAudioIndex != -1)
		return 0;

	if(avformat_open_input(&fFormatCtx, fInputFileName, NULL, NULL) != 0)
		return -1; // Couldn't open file

	// Retrieve stream information
	if(av_find_stream_info(fFormatCtx) < 0)
		return -1; // Couldn't find stream information
	
	//  print file information
	//dump_format(fFormatCtx, 0, fInputFileName, 0);

	for(unsigned int i = 0; i < fFormatCtx->nb_streams; i++) {
		if(fFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			fVideoIndex = i;
			AVRational time_base = fFormatCtx->streams[i]->time_base;
			fMicroSecPerFrame = 1000000L * (uint64_t)time_base.num / (uint64_t)time_base.den;

			//  fix config data
			if (fFormatCtx->streams[i]->codec->extradata != 0) {
				updateUserData(fFormatCtx->streams[i]->codec->extradata, fFormatCtx->streams[i]->codec->extradata_size);
			}
		}
		else if(fFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO){
			fAudioIndex = i;
		}

		if (fVideoIndex >=0 && fAudioIndex >= 0) {
			break;
		}
	}

	char const* extension = strrchr(fInputFileName, '.');
	if (strcmp(extension, ".mp4") == 0 && fVideoIndex != -1 &&
			fFormatCtx->streams[fVideoIndex]->codec->codec_id == AV_CODEC_ID_H264){
		fUseH264BSF = True;
		fH264BSF = av_bitstream_filter_init("h264_mp4toannexb");
	}else {
		fUseH264BSF = False;
		fH264BSF = NULL;
	}

	//  parse the AVI file successfully
	printf("#AVIDemux# prepare() fVideoIndex = %d, fAudioIndex = %d\n", fVideoIndex, fAudioIndex);
	return 0; 
}


//  AVI duration
float AVIDemux::getDuration()
{
	//return fAVIHeader.dwTotalFrames * fAVIHeader.dwMicroSecPerFrame / 1000000.0;
	if (fFormatCtx) {
		return fFormatCtx->duration / 1000000.0;
	}
	else {
		return 0.0;
	}
}

//int AVIDemux::seekStream(float pos)

//  Seek to Time(seconds), if successful
int AVIDemux::seekStream(float pos)
{
	if (pos < 0 || pos > getDuration()) {
		return -1;
	}
	
	//  clear buffers
	
#if 1
	int ret;
	ret = avformat_seek_file(fFormatCtx, -1, INT64_MIN, (int64_t)(pos * AV_TIME_BASE), INT64_MAX, 0);
	if (ret < 0) {
		printf("#AVIDemux# seekStream() seek error!!\n");
	} else {
		for (unsigned i = 0; i < MAX_AVI_STREAM; ++i) {
			delete fOutput[i].savedDataHead;
			fOutput[i].savedDataHead = NULL;
		}
		AVPacket packet;
		av_init_packet(&packet);
		av_read_frame(fFormatCtx, &packet);
		printf("#AVIDemux# seekStream() 0x%x, seek pos:%f\n", this, packet.pts*av_q2d(fFormatCtx->streams[packet.stream_index]->time_base));
	}
	
#else
	int64_t start_pos;  //  file position
	uint64_t timestamp = pos * 1000000L;
	if (timestamp == 0) {
		av_seek_frame(fFormatCtx, -1, 0, AVSEEK_FLAG_BYTE);
		fFirst_vop = True;
		fOutput[0].presentationTime.tv_sec = fOutput[0].presentationTime.tv_usec = 0;
		return 0;
	}

	if (av_seek_frame(fFormatCtx, -1, timestamp, 0) == -1) {
		return -1;
	}

	//  store the file position of the seek point
	start_pos = avio_tell(fFormatCtx->pb);
		
	//  only one stream, no need to so video/audio time synchronize 
	if (fVideoIndex < 0 || fAudioIndex < 0) {
		return 0;
	}

	// try to get the fisrt video/audio packet
	AVPacket *video_pkt = NULL;
	AVPacket *audio_pkt = NULL;
	AVPacket packet;

	//  read out the first video/audio packet
	while(1) {
		av_init_packet(&packet);
		if (av_read_frame(fFormatCtx, &packet) != 0 ) {
			break;
		}

		if (packet.stream_index == fVideoIndex && video_pkt == NULL) {
			video_pkt = new AVPacket;
			av_dup_packet(&packet);
			memcpy(video_pkt, &packet, sizeof(AVPacket));
		}
		else if (packet.stream_index == fAudioIndex && audio_pkt == NULL) {
			audio_pkt = new AVPacket;
			av_dup_packet(&packet);
			memcpy(audio_pkt, &packet, sizeof(AVPacket));
		}
		else {
			av_free_packet(&packet);
		}
		
		//  The first video and audio packet are got
		if (video_pkt && audio_pkt) {
			break;
		}
	}

	int need_re_parse = 0;
	//  check if the first video packet is key-frame
	if (video_pkt && isKeyFrame(video_pkt) != True) {
		need_re_parse = 1;
	}

	// the first 
	if (need_re_parse) {
		if (video_pkt) {
			av_free_packet(video_pkt);
			delete video_pkt;
			video_pkt = NULL;
		}
		if (audio_pkt) {
			av_free_packet(audio_pkt);
			delete audio_pkt;
			audio_pkt = NULL;
		}
		
		//  start search a key-frame
		while(1) {
			if (av_read_frame(fFormatCtx, &packet) != 0 ) {
				break;
			}

			if (packet.stream_index == fVideoIndex) {
				if (isKeyFrame(&packet) == True) {
					video_pkt = new AVPacket;
					av_dup_packet(&packet);
					memcpy(video_pkt, &packet, sizeof(AVPacket));
					start_pos = avio_tell(fFormatCtx->pb);
					break;
				}
			}

			av_free_packet(&packet);
		}

		//  after get a key-frame, get the next audio packet
		while(1) {
			if (av_read_frame(fFormatCtx, &packet) != 0 ) {
				break;
			}

			if (packet.stream_index == fAudioIndex) {
				audio_pkt = new AVPacket;
				av_dup_packet(&packet);
				memcpy(audio_pkt, &packet, sizeof(AVPacket));
				break;
			}
			av_free_packet(&packet);
		}
				
	}


	if (video_pkt && audio_pkt) {
		double v_pts, a_pts;
		v_pts = convertPTS(video_pkt);
		a_pts = convertPTS(audio_pkt);
		
		fVideo_diff = v_pts - a_pts;

		//  According  to ITU-R BT.1359, the reliable detection is between 45msec audio leading video and 125msec audio lagging behind video, 
		//  the tolerance from the point of capture to the viewer and or listener shall be no more than 90msec (audio leading video) and 185msec (audio lagging behind video).
		if (fVideo_diff > 0.045 || fVideo_diff < -0.045) {
			//  av_seek_frame(fFormatCtx, -1, timestamp, 0);
			//  seek a little earlier, assume 10 seconds is enought for the previous key-frame of video
			
			if (fVideo_diff < 0) {
				if (v_pts > 10.0) {
					timestamp = (v_pts - 10.0) * 1000000L;
				}
				else {
					timestamp = 0;
				}
			}

			//  start read some packet and check the pts
			av_seek_frame(fFormatCtx, -1, timestamp, 0);
			while (1) {
				if (av_read_frame(fFormatCtx, &packet) != 0 ) {
					break;
				}

				if (fVideo_diff > 0) {  //  we should save some video packets until audio pts 
					if (packet.stream_index == fVideoIndex) {
						if (convertPTS(&packet) < convertPTS(video_pkt)) {
							av_free_packet(&packet);
							continue;
						}
						else {
							//  save packet
							//  for a Video packet, there maybe two MPEG4 VOP in it
							int first_vop_pos = 0;
							int second_vop_pos = 0;
							int second_size = 0;
							if (getCodecID(fVideoIndex) == CODEC_ID_MPEG4) {
								if (checkVOPCoded(packet.data, packet.size) == False) {
								//	printf("drop one packet \n");
									av_free_packet(&packet);
									continue;
								}
								
								packet.size = updateUserData(packet.data, packet.size);

								//  try to unpack some packed-bitstreams
								first_vop_pos  = findVOPStartCode(packet.data, 0, packet.size);   // find the first 0x000001B6, always successful
								second_vop_pos = findVOPStartCode(packet.data, first_vop_pos + 1, packet.size);// failed most of the time			
								second_size = packet.size - second_vop_pos;
								packet.size = second_vop_pos; 
							}
							
							struct OutputDescriptor& out = fOutput[0];

							unsigned char* buf = new unsigned char[packet.size];
							memcpy(buf, packet.data, packet.size);
							AVIDemux::OutputDescriptor::SavedData* savedData
								= new AVIDemux::OutputDescriptor::SavedData(buf, packet.size, packet.pts);
							if (out.savedDataHead == NULL) {
								out.savedDataHead = out.savedDataTail = savedData;
							} else {
								out.savedDataTail->next = savedData;
								out.savedDataTail = savedData;
							}
							out.savedDataTotalSize += packet.size;

							if (getCodecID(fVideoIndex) == CODEC_ID_MPEG4) {
								if (second_size > 0) {
									struct OutputDescriptor& out = fOutput[0];

									unsigned char* buf = new unsigned char[second_size];
									memcpy(buf, packet.data + second_vop_pos, second_size);
									AVIDemux::OutputDescriptor::SavedData* savedData
										= new AVIDemux::OutputDescriptor::SavedData(buf, second_size, packet.pts);
									if (out.savedDataHead == NULL) {
										out.savedDataHead = out.savedDataTail = savedData;
									} else {
										out.savedDataTail->next = savedData;
										out.savedDataTail = savedData;
									}
									out.savedDataTotalSize += second_size;
								}
							}
							av_free_packet(&packet);
						}
					}
					else if (packet.stream_index == fAudioIndex) {
						if (convertPTS(&packet) < convertPTS(video_pkt))  {
							av_free_packet(&packet);
							continue;
						}
						else {
							struct OutputDescriptor& out = fOutput[1];

							unsigned char* buf = new unsigned char[packet.size];
							memcpy(buf, packet.data, packet.size);
							AVIDemux::OutputDescriptor::SavedData* savedData
								= new AVIDemux::OutputDescriptor::SavedData(buf, packet.size, packet.pts);
							if (out.savedDataHead == NULL) {
								out.savedDataHead = out.savedDataTail = savedData;
							} else {
								out.savedDataTail->next = savedData;
								out.savedDataTail = savedData;
							}
							out.savedDataTotalSize += packet.size;

							fVideo_diff = v_pts - convertPTS(&packet);
							av_free_packet(&packet);
							break;
						}
					}	
				}
				else {   // fVideo_diff < 0, save some previous audio packets
					if (packet.stream_index == fAudioIndex) {
						if (convertPTS(&packet) < convertPTS(video_pkt))  {
							av_free_packet(&packet);
							continue;
						}
						else if (packet.pts < audio_pkt->pts || packet.dts < audio_pkt->dts) {
							struct OutputDescriptor& out = fOutput[1];

							unsigned char* buf = new unsigned char[packet.size];
							memcpy(buf, packet.data, packet.size);
							int64_t pts = (packet.pts != AV_NOPTS_VALUE) ? packet.pts : packet.dts ;
							AVIDemux::OutputDescriptor::SavedData* savedData
								= new AVIDemux::OutputDescriptor::SavedData(buf, packet.size, pts);
							if (out.savedDataHead == NULL) {
								out.savedDataHead = out.savedDataTail = savedData;
							} else {
								out.savedDataTail->next = savedData;
								out.savedDataTail = savedData;
							}
							out.savedDataTotalSize += packet.size;
							av_free_packet(&packet);
						}
					}
					else if (packet.stream_index == fVideoIndex) {
						if (convertPTS(&packet) < convertPTS(video_pkt))  {
							av_free_packet(&packet);
							continue;
						}
						else {
							{
								int first_vop_pos = 0;
								int second_vop_pos = 0;
								int second_size = 0;
								if (getCodecID(fVideoIndex) == CODEC_ID_MPEG4) {
									if (checkVOPCoded(packet.data, packet.size) == False) {
										//	printf("drop one packet \n");
										av_free_packet(&packet);
										continue;
									}

									packet.size = updateUserData(packet.data, packet.size);

									//  try to unpack some packed-bitstreams
									first_vop_pos  = findVOPStartCode(packet.data, 0, packet.size);   // find the first 0x000001B6, always successful
									second_vop_pos = findVOPStartCode(packet.data, first_vop_pos + 1, packet.size);// failed most of the time			
									second_size = packet.size - second_vop_pos;
									packet.size = second_vop_pos; 
								}

								struct OutputDescriptor& out = fOutput[0];

								unsigned char* buf = new unsigned char[packet.size];
								memcpy(buf, packet.data, packet.size);
								AVIDemux::OutputDescriptor::SavedData* savedData
									= new AVIDemux::OutputDescriptor::SavedData(buf, packet.size, packet.pts);
								if (out.savedDataHead == NULL) {
									out.savedDataHead = out.savedDataTail = savedData;
								} else {
									out.savedDataTail->next = savedData;
									out.savedDataTail = savedData;
								}
								out.savedDataTotalSize += packet.size;

								if (getCodecID(fVideoIndex) == CODEC_ID_MPEG4) {
									if (second_size > 0) {
										struct OutputDescriptor& out = fOutput[0];

										unsigned char* buf = new unsigned char[second_size];
										memcpy(buf, packet.data + second_vop_pos, second_size);
										AVIDemux::OutputDescriptor::SavedData* savedData
											= new AVIDemux::OutputDescriptor::SavedData(buf, second_size, packet.pts);
										if (out.savedDataHead == NULL) {
											out.savedDataHead = out.savedDataTail = savedData;
										} else {
											out.savedDataTail->next = savedData;
											out.savedDataTail = savedData;
										}
										out.savedDataTotalSize += second_size;
									}
								}
								av_free_packet(&packet);
							}

							struct OutputDescriptor& out = fOutput[1];
							OutputDescriptor::SavedData* savedData = out.savedDataHead;
							
							if (savedData != NULL) {
								double a_pts = (double)(savedData->pts * fFormatCtx->streams[fAudioIndex]->time_base.num) / fFormatCtx->streams[fAudioIndex]->time_base.den;
								fVideo_diff = convertPTS(video_pkt) - a_pts;;
							}
							
							avio_seek(fFormatCtx->pb, start_pos, SEEK_SET);
						
							av_free_packet(&packet);
							break;
						}
					}
				}
			}
		}
		else {
			avio_seek(fFormatCtx->pb, start_pos, SEEK_SET);
		}

	}
	
	if (video_pkt) {
		av_free_packet(video_pkt);
		delete video_pkt;
	}
	if (audio_pkt) {
		av_free_packet(audio_pkt);
		delete audio_pkt;
	}

	//  reset some variables for PTS correction
	if (fVideoIndex >= 0) {
		fFirst_vop = True;
		fOutput[0].presentationTime.tv_sec = fOutput[0].presentationTime.tv_usec = 0;
	}
#endif
		
	return 0;
}


//  microseconds for one video frame
u_int32_t AVIDemux::getFrameTime()
{
	return fMicroSecPerFrame;
}

//  A/V codec ID, all types are defined in ffmpeg/libavcodec/avcodec.h
u_int32_t AVIDemux::getCodecID(char streamIndex)
{	
	if (streamIndex >= 20)
		return -1;
	//printf("#AVIDemux# getCodecID() index = %d\n", streamIndex);
	AVStream *st = fFormatCtx->streams[streamIndex];

	return st->codec->codec_id;
}


//  A/V codec config data, some kinds of codec(like AVC, AAC, MPEG4) have config data
void AVIDemux::getConfigData(char streamIndex, uint8_t **data, int *size)
{	
	AVStream *st = fFormatCtx->streams[streamIndex];
	*data = st->codec->extradata;
	*size = st->codec->extradata_size;
	return;
}

u_int32_t AVIDemux::getSamplingRate()
{
	if (fAudioIndex >=0) {
		AVStream* audio = fFormatCtx->streams[fAudioIndex];
		return audio->codec->sample_rate;
	}
	else {
		return 0;
	}
}

unsigned int AVIDemux::getNumChannels()
{
	if (fAudioIndex >=0) {
		AVStream* audio = fFormatCtx->streams[fAudioIndex];
		return audio->codec->channels;
	}
	else {
		return 0;
	}
}

unsigned int AVIDemux::getBitrate(char streamIndex)
{
	if (streamIndex < 0 || streamIndex >=  fFormatCtx->nb_streams)
		return 0;

	if (streamIndex == fVideoIndex) {
		if(fAudioIndex >= 0) {
			AVStream* audio = fFormatCtx->streams[fAudioIndex];
			return (fFormatCtx->bit_rate - audio->codec->bit_rate) / 1000;//  ?? 1024
		}
		else {
			return fFormatCtx->bit_rate / 1000;
		}
	}

	if (streamIndex == fAudioIndex) {
		AVStream* audio = fFormatCtx->streams[fAudioIndex];
		int bits_per_sample = av_get_bits_per_sample(audio->codec->codec_id);
		return (bits_per_sample ? audio->codec->sample_rate * audio->codec->channels * 2 : audio->codec->bit_rate) / 1000; // ??
	}

	return -1;
}


AVIDemuxedElementaryStream*
AVIDemux::newElementaryStream(char streamIndex) {
	return new AVIDemuxedElementaryStream(envir(), streamIndex, *this);
}

AVIDemuxedElementaryStream* AVIDemux::newAudioStream() {
	if (fAudioIndex >= 0) {
		return newElementaryStream(fAudioIndex);
	}
	else { 
		return NULL;
	}
}

AVIDemuxedElementaryStream* AVIDemux::newVideoStream() {
	if (fVideoIndex >= 0) {
		return newElementaryStream(fVideoIndex);
	}
	else { 
		return NULL;
	}
}

void AVIDemux::registerReadInterest(char streamIndex,
									unsigned char* to, unsigned maxSize,
									FramedSource::afterGettingFunc* afterGettingFunc,
									void* afterGettingClientData,
									FramedSource::onCloseFunc* onCloseFunc,
									void* onCloseClientData) 
{
	struct OutputDescriptor& out = fOutput[streamIndex == fVideoIndex ? 0 : 1];

	// Make sure this stream is not already being read:
	if (out.isCurrentlyAwaitingData) {
		envir() << "MPEG1or2Demux::registerReadInterest(): attempt to read stream id "
			<< (void*)streamIndex << " more than once!\n";
		envir().internalError();
	}

	out.to = to; out.maxSize = maxSize;
	out.fAfterGettingFunc = afterGettingFunc;
	out.afterGettingClientData = afterGettingClientData;
	out.fOnCloseFunc = onCloseFunc;
	out.onCloseClientData = onCloseClientData;
	out.isCurrentlyActive = True;
	out.isCurrentlyAwaitingData = True;
	// out.frameSize and out.presentationTime will be set when a frame's read

}

Boolean AVIDemux::useSavedData(char streamIndex,
									unsigned char* to, unsigned maxSize,
									FramedSource::afterGettingFunc* afterGettingFunc,
									void* afterGettingClientData) 
{
	struct OutputDescriptor& out = fOutput[streamIndex == fVideoIndex ? 0 : 1];
	if (out.savedDataHead == NULL) return False; // common case
	int64_t pts;

	unsigned totNumBytesCopied = 0;
	if (maxSize > 0 && out.savedDataHead != NULL) {
		OutputDescriptor::SavedData& savedData = *(out.savedDataHead);
		unsigned char* from = &savedData.data[0];
		unsigned numBytesToCopy = savedData.dataSize;
		if (numBytesToCopy > maxSize) {
			numBytesToCopy = maxSize;
			printf("Packet is too Large, drops some data \n");
		}
		memcpy(to, from, numBytesToCopy);
		maxSize -= numBytesToCopy;
		out.savedDataTotalSize -= numBytesToCopy;
		totNumBytesCopied += numBytesToCopy;

		pts = savedData.pts;
		
		out.savedDataHead = savedData.next;
		if (out.savedDataHead == NULL) out.savedDataTail = NULL;
		savedData.next = NULL;
		delete &savedData;	
	}

	out.isCurrentlyActive = True;
#ifdef DEMUX_DEBUG	
	char buf[256];
	sprintf(buf, "useSavedData() Index(%d) totNumBytesCopied %d \n", streamIndex, totNumBytesCopied);
//	envir()<< buf << "\n";
//	OutputDebugStringA(buf);
#endif

	if (afterGettingFunc != NULL) {
		struct timeval presentationTime = { 0, 0 };

		if (fVideoIndex == streamIndex) {		
			struct OutputDescriptor& out = fOutput[0];
			out.fAfterGettingFunc = afterGettingFunc;
			out.afterGettingClientData = afterGettingClientData;
			sendVideoFrame(to, totNumBytesCopied);
		}
		else {
			AVCodecContext *codec = fFormatCtx->streams[fAudioIndex]->codec;
			(*afterGettingFunc)(afterGettingClientData,
				totNumBytesCopied, 
				0,
				presentationTime,
				codec->frame_size * 1000000L / codec->sample_rate);
		}
	}
	return True;
}

void AVIDemux::GetFrame(AVPacket *packet)
{
	av_read_frame(fFormatCtx, packet);
}

void AVIDemux::continueReadProcessing(char streamIndex,
									  unsigned char* to, unsigned maxSize,
									  FramedSource::afterGettingFunc* afterGettingFunc,
									  void* afterGettingClientData,
									  FramedSource::onCloseFunc* onCloseFunc,
									  void* onCloseClientData) 
{
	AVPacket packet;
	av_init_packet(&packet);
	do {
		//GetFrame(&packet);

		if (av_read_frame(fFormatCtx, &packet) < 0) {
			// no packet, maybe reach the end of file
			(*onCloseFunc)(onCloseClientData);
			break;
		}

		int acquiredStreamIndex = packet.stream_index;
		/*if (fVideoIndex == acquiredStreamIndex) {
			printf("%x\t", this);
			printf("#continueReadProcessing# current pos:%f\n", packet.pts*av_q2d(fFormatCtx->streams[fVideoIndex]->time_base));
		}*/
		if (acquiredStreamIndex == streamIndex) {
			// We were able to acquire a frame from the input.
			unsigned numBytesToCopy;

			if (fVideoIndex == acquiredStreamIndex && getCodecID(fVideoIndex) == CODEC_ID_MPEG4) {
				int first_vop_pos = 0;
				int second_vop_pos = 0;
				
				if (checkVOPCoded(packet.data, packet.size) == False) {
				//	printf("drop one packet \n");
					av_free_packet(&packet);
					continue;
				}

				packet.size = updateUserData(packet.data, packet.size);

				//  try to unpack some packed-bitstreams
				first_vop_pos  = findVOPStartCode(packet.data, 0, packet.size);   // find the first 0x000001B6, always successful
				second_vop_pos = findVOPStartCode(packet.data, first_vop_pos + 1, packet.size);// failed most of the time

				if (second_vop_pos < packet.size) {
					struct OutputDescriptor& out = fOutput[acquiredStreamIndex == fVideoIndex ? 0 : 1];

					unsigned char* buf = new unsigned char[packet.size - second_vop_pos];
					memcpy(buf, packet.data + second_vop_pos, packet.size - second_vop_pos);
					AVIDemux::OutputDescriptor::SavedData* savedData
						= new AVIDemux::OutputDescriptor::SavedData(buf, packet.size - second_vop_pos, packet.pts);
					if (out.savedDataHead == NULL) {
						out.savedDataHead = out.savedDataTail = savedData;
					} else {
						out.savedDataTail->next = savedData;
						out.savedDataTail = savedData;
					}
					out.savedDataTotalSize += packet.size - second_vop_pos;
					packet.size = second_vop_pos;
				}
			}

			if(fVideoIndex == acquiredStreamIndex && fUseH264BSF && fH264BSF){
				av_bitstream_filter_filter(fH264BSF, fFormatCtx->streams[fVideoIndex]->codec, 
							NULL, &packet.data, &packet.size, packet.data, packet.size, 0);
			}

			if ((unsigned)packet.size > maxSize) {
				envir() << "AVIStreamParser::continueReadProcessing() error: packet_length ("
					<< packet.size
					<< ") exceeds max frame size asked for ("
					<< maxSize << ")\n";
				numBytesToCopy = maxSize;
			} else {
				numBytesToCopy = packet.size;
			}

			memcpy(to, packet.data, numBytesToCopy);

			

			// Call our own 'after getting' function.  Because we're not a 'leaf'
			// source, we can call this directly, without risking infinite recursion.
			if (afterGettingFunc != NULL) {
				struct timeval presentationTime;

				presentationTime.tv_sec = presentationTime.tv_usec = 0;

				if (fVideoIndex == acquiredStreamIndex) {		
					struct OutputDescriptor& out = fOutput[0];
					out.fAfterGettingFunc = afterGettingFunc;
					out.afterGettingClientData = afterGettingClientData;
					out.fOnCloseFunc = onCloseFunc;
					out.onCloseClientData = onCloseClientData;
					sendVideoFrame(to, numBytesToCopy);
				}
				else {
					AVCodecContext *codec = fFormatCtx->streams[fAudioIndex]->codec;
					(*afterGettingFunc)(afterGettingClientData,
						numBytesToCopy, 
						0,
						presentationTime,
						codec->frame_size * 1000000L / codec->sample_rate);
				}
			}
			av_free_packet(&packet);
			return;
		}
		else {
			//  for a Video packet, there maybe two MPEG4 VOP in it
			int first_vop_pos = 0;
			int second_vop_pos = 0;
			int second_size = 0;
			
			//  not desired stream, drop it and continue
			if (acquiredStreamIndex != fVideoIndex && acquiredStreamIndex != fAudioIndex) {
				av_free_packet(&packet);
				continue;
			}
			
			if (fVideoIndex == acquiredStreamIndex && getCodecID(fVideoIndex) == CODEC_ID_MPEG4) {

				if (checkVOPCoded(packet.data, packet.size) == False) {
				//	printf("drop one packet \n");
					av_free_packet(&packet);
					continue;
				}
				
				packet.size = updateUserData(packet.data, packet.size);

				//  try to unpack some packed-bitstreams
				first_vop_pos  = findVOPStartCode(packet.data, 0, packet.size);   // find the first 0x000001B6, always successful
				second_vop_pos = findVOPStartCode(packet.data, first_vop_pos + 1, packet.size);// failed most of the time			
				second_size = packet.size - second_vop_pos;
				packet.size = second_vop_pos; 
			}
			
			struct OutputDescriptor& out = fOutput[acquiredStreamIndex == fVideoIndex ? 0 : 1];

			unsigned char* buf = new unsigned char[packet.size];
			memcpy(buf, packet.data, packet.size);
			AVIDemux::OutputDescriptor::SavedData* savedData
				= new AVIDemux::OutputDescriptor::SavedData(buf, packet.size, packet.pts);
			if (out.savedDataHead == NULL) {
				out.savedDataHead = out.savedDataTail = savedData;
			} else {
				out.savedDataTail->next = savedData;
				out.savedDataTail = savedData;
			}
			out.savedDataTotalSize += packet.size;

			if (fVideoIndex == acquiredStreamIndex && getCodecID(fVideoIndex) == CODEC_ID_MPEG4) {
				if (second_size > 0) {
					struct OutputDescriptor& out = fOutput[0];

					unsigned char* buf = new unsigned char[second_size];
					memcpy(buf, packet.data + second_vop_pos, second_size);
					AVIDemux::OutputDescriptor::SavedData* savedData
						= new AVIDemux::OutputDescriptor::SavedData(buf, second_size, packet.pts);
					if (out.savedDataHead == NULL) {
						out.savedDataHead = out.savedDataTail = savedData;
					} else {
						out.savedDataTail->next = savedData;
						out.savedDataTail = savedData;
					}
					out.savedDataTotalSize += second_size;
				}
			}
			av_free_packet(&packet);
		}
	} while(1);

	//  end
}


//  some AVI files have packed bitstream, unpack it first. Ref to the src of avidemux (http://fixounet.free.fr/avidemux/)
int AVIDemux::updateUserData(unsigned char* data, unsigned size)
{
	unsigned i;
	for (i = 0; i < size - 4; ++i) {
		//  find start code 0x000001B2
		if (data[i] == 0 && data[i+1] == 0 && data[i+2] == 1 && data[i+3] == 0xb2) {
			uint8_t * start = data + i + 4;
            uint32_t  rlen = size - i - 4;
			if (!strncmp((const char *)start, "DivX", 4)) {
				start += 4;
				rlen -= 4; // skip "DivX"
				// looks for a p while not null
				// if there isn't we will reach a new start code
				// and it will stop
				while((*start!='p') && rlen) 
				{
					if(!*start)
					{
						rlen=0;
						break;
					}
					rlen--;
					i++;
					start++;
				}
				if(!rlen) {
					//printf("Unpacketizer:packed marker not found!\n");
				}
				else	
					*start='n'; // remove 'p'		
			}
		}
	}

	return size;
}


//  check if a packet is a key-frame
Boolean AVIDemux::isKeyFrame(AVPacket * pkt)
{
	if((pkt->stream_index != fVideoIndex) || (pkt->size == 0)) {
		return False;
	}
	
	//  ffmpeg has marked the key-frame flag
	if (pkt->flags & AV_PKT_FLAG_KEY) {
		return True;
	}
	else {
		return False;
	}

	/*
	if(getCodecID(fVideoIndex) == CODEC_ID_MPEG4) {
		int start = findVOPStartCode(pkt->data, 0, pkt->size);
		if (start < pkt->size - 4) {
			u_int8_t vop_coding_type = pkt->data[start + 4] >> 6;
			if (vop_coding_type == 0) {  // I-frame 
				return True;
			}
			else {
				return False;
			}
		}
	}
	else {
		return True;
	}
	*/

	return False;
}


int AVIDemux::findVOPStartCode(unsigned char* data, int start_pos, int size)
{	
	int i;
	for (i = start_pos; i < size - 4; ++i) {
		if (data[i] == 0 && data[i+1] == 0 && data[i+2] == 1 && data[i+3] == 0xb6) {
			return i;
		}
	}
	return i + 4;
}

void AVIDemux::sendVideoFrame(unsigned char* data, unsigned size)
{
	struct timeval presentationTime;
	struct OutputDescriptor& out = fOutput[0];
	int frameDuration = 0;

	if (getCodecID(fVideoIndex) == CODEC_ID_MPEG4) {
		fixMPEG4PTS(data, size);

		if (fOutput[0].presentationTime.tv_sec  == 0 && 
			fOutput[0].presentationTime.tv_usec == 0) {
			gettimeofday(&fOutput[0].presentationTime, NULL);
			double pts = fOutput[0].presentationTime.tv_sec + fOutput[0].presentationTime.tv_usec / 1000000.0;
			pts += fVideo_diff;
			fOutput[0].presentationTime.tv_sec  = (int) pts;
			fOutput[0].presentationTime.tv_usec = (pts - (double)fOutput[0].presentationTime.tv_sec) * 1000000L;

			presentationTime = fOutput[0].presentationTime;

			fVideo_diff = 0.0;
			frameDuration = fMicroSecPerFrame;
		}
		else {
			uint64_t pts_ms = fLast_fixed_pts * 1000000L / (uint64_t)vop_time_increment_resolution;
			uint64_t t_usec = fOutput[0].presentationTime.tv_usec + pts_ms;
			presentationTime.tv_sec  = fOutput[0].presentationTime.tv_sec + t_usec / 1000000L;
			presentationTime.tv_usec = t_usec % 1000000L;

			if (fFrameMissing) {
				frameDuration = fMicroSecPerFrame + fFrameMissing * 1000000L / vop_time_increment_resolution;
			}
			else {
				frameDuration = fMicroSecPerFrame;
			}	
		}
	}
	else {
		if (fOutput[0].presentationTime.tv_sec  == 0 && 
			fOutput[0].presentationTime.tv_usec == 0) {
				gettimeofday(&fOutput[0].presentationTime, NULL);
				presentationTime = fOutput[0].presentationTime;
		}
		else {
		//	uint64_t pts_ms = fLast_fixed_pts * 1000000L / (uint64_t)vop_time_increment_resolution;
		//	uint64_t t_usec = fOutput[0].presentationTime.tv_usec + pts_ms;
			presentationTime = fOutput[0].presentationTime;
			presentationTime.tv_usec += fMicroSecPerFrame;
			presentationTime.tv_sec  += presentationTime.tv_usec / 1000000L;
			presentationTime.tv_usec =  presentationTime.tv_usec % 1000000L;	
			fOutput[0].presentationTime = presentationTime;
		}
		frameDuration = fMicroSecPerFrame;
	}
#ifdef DEMUX_DEBUG
	printf("fLast_fixed_pts = %lld size %d, fVop_coded = %d sendPTS = %d.%06d, %d \n", 
			fLast_fixed_pts, size, fVop_coded, presentationTime.tv_sec, presentationTime.tv_usec, fMicroSecPerFrame);
#endif

	(*out.fAfterGettingFunc)(out.afterGettingClientData,
		size, 
		0,
		presentationTime,
		frameDuration);
}

typedef  struct ReadParameters {
	// input parameters
	AVIDemux * demux;
	char streamIndex;
	unsigned char* to; 
	unsigned maxSize;
	FramedSource::afterGettingFunc* fAfterGettingFunc;
	void* afterGettingClientData;
	FramedSource::onCloseFunc* fOnCloseFunc;
	void* onCloseClientData;
} TagReadParameters;

void AVIDemux::continueReadProcessing(void* clientData) {
	ReadParameters* para = (ReadParameters*)clientData;

	// Next, if we're the only currently pending read, continue looking for data:
	para->demux->continueReadProcessing(para->streamIndex, para->to, para->maxSize,
		para->fAfterGettingFunc, para->afterGettingClientData,
		para->fOnCloseFunc, para->onCloseClientData);
	// otherwise the continued read processing has already been taken care of
}

void AVIDemux::getNextFrame(char streamIndex,
							 unsigned char* to, unsigned maxSize,
							 FramedSource::afterGettingFunc* afterGettingFunc,
							 void* afterGettingClientData,
							 FramedSource::onCloseFunc* onCloseFunc,
							 void* onCloseClientData) 
{
	// First, check whether we have saved data for this stream id:
	if (useSavedData(streamIndex, to, maxSize,
				afterGettingFunc, afterGettingClientData)) {
		return;
	}

#if 0
	ReadParameters *para = new ReadParameters();

	para->demux = this;
	para->streamIndex = streamIndex;
	para->to = to; 
	para->maxSize = maxSize;
	para->fAfterGettingFunc = afterGettingFunc;
	para->afterGettingClientData = afterGettingClientData;
	para->fOnCloseFunc = onCloseFunc;
	para->onCloseClientData = onCloseClientData;

	envir().taskScheduler().scheduleDelayedTask(0,
		(TaskFunc*)AVIDemux::continueReadProcessing, para);
#else
	// Next, if we're the only currently pending read, continue looking for data:
	continueReadProcessing(streamIndex, to, maxSize,
		afterGettingFunc, afterGettingClientData,
		onCloseFunc, onCloseClientData);
#endif
	// otherwise the continued read processing has already been taken care of
}

void AVIDemux::stopGettingFrames(char streamIndex) {
	struct OutputDescriptor& out = fOutput[streamIndex == fVideoIndex ? 0 : 1];
	out.isCurrentlyActive = out.isCurrentlyAwaitingData = False;
	if (fVideoIndex >= 0) {
		fFirst_vop = True;
		fOutput[0].presentationTime.tv_sec = fOutput[0].presentationTime.tv_usec = 0;
	}
}

void AVIDemux::fixMPEG4PTS(unsigned char *framedata, unsigned frameSize)
{	
	if (frameSize >= 4 && framedata[0] == 0 && framedata[1] == 0 && framedata[2] == 1) {
		unsigned i = 3;
		if (framedata[i] == 0xB0 || framedata[i] == 0x00) { // VISUAL_OBJECT_SEQUENCE_START_CODE
			// The start of this frame - up to the first GROUP_VOP_START_CODE
			// or VOP_START_CODE - is stream configuration information.  Save this:
			for (i = 7; i < frameSize; ++i) {
				if ((framedata[i] == 0xB3 /*GROUP_VOP_START_CODE*/ ||
					framedata[i] == 0xB6 /*VOP_START_CODE*/)
					&& framedata[i-1] == 1 && framedata[i-2] == 0 && framedata[i-3] == 0) {
					break; // The configuration information ends here
				}
			}

			fConfigBytes = framedata;
			fNumConfigBytes = frameSize;

			analyzeVOLHeader();
		}

		if (i < frameSize) {
			u_int8_t nextCode = framedata[i];

			if (nextCode == 0xB3 /*GROUP_VOP_START_CODE*/) {
				// Skip to the following VOP_START_CODE (if any):
				for (i += 4; i < frameSize; ++i) {
					if (framedata[i] == 0xB6 /*VOP_START_CODE*/
						&& framedata[i-1] == 1 && framedata[i-2] == 0 && framedata[i-3] == 0) {
						nextCode = framedata[i];
						break;
					}
				}
			}

			if (nextCode == 0xB6 /*VOP_START_CODE*/ && i+3 <= frameSize) {
				++i;

				// Get the "vop_coding_type" from the next byte:
				u_int8_t nextByte = framedata[i++];
				u_int8_t vop_coding_type = nextByte>>6;

				// Next, get the "modulo_time_base" by counting the '1' bits that
				// follow.  We look at the next 32-bits only.
				// This should be enough in most cases.
			
				int fetchBytes = (frameSize - i) > 4 ? 4 : (frameSize - i);
				u_int32_t next4Bytes = 0;
				for (int j = 0; j < fetchBytes; j++) {
					next4Bytes |= framedata[i + j] << (24 - j * 8);
				}
				
				//u_int32_t next4Bytes
				//	= (framedata[i]<<24)|(framedata[i+1]<<16)|(framedata[i+2]<<8)|framedata[i+3];
				//i += 4;
				u_int32_t timeInfo = (nextByte<<(32-6))|(next4Bytes>>6);
				unsigned modulo_time_base = 0;
				u_int32_t mask = 0x80000000;
				while ((timeInfo&mask) != 0) {
					++modulo_time_base;
					mask >>= 1;
				}
				mask >>= 2;

				// Then, get the "vop_time_increment".
				unsigned vop_time_increment = 0;
				// First, make sure we have enough bits left for this:
				if ((mask>>(fNumVTIRBits-1)) != 0) {
					for (unsigned i = 0; i < fNumVTIRBits; ++i) {
						vop_time_increment |= timeInfo&mask;
						mask >>= 1;
					}
					
					fVop_coded = timeInfo&(mask>>1); 
					while (mask != 0) {
						vop_time_increment >>= 1;
						mask >>= 1;
					}
				}

				if (fFirst_vop == True) {
					fLast_vop_time_increment = vop_time_increment;
					fLast_fixed_pts = 0;
					fFirst_vop = False;

					fFrameMissing = 0;
					fLastNonBFrameVop_time_increment = 0;
					fLastNonBFrameInc = 0;
				}

				if (fLast_vop_time_increment <= vop_time_increment) { 
					if(fLast_vop_coding_type != 2 && vop_coding_type == 2) {   //  B-frame
						fLast_fixed_pts =  fLast_fixed_pts + vop_time_increment - fLast_vop_time_increment - vop_time_increment_resolution;
						fLast_vop_coding_type = vop_coding_type;
						fLast_vop_time_increment = vop_time_increment;
					}
					else {  //  normal situation
						fLast_fixed_pts +=  vop_time_increment - fLast_vop_time_increment;
						fLast_vop_coding_type = vop_coding_type;
						fLast_vop_time_increment = vop_time_increment;
					}
				}
				else {
					if(fLast_vop_coding_type != 2 && vop_coding_type == 2){  
						//  Last frame is P-frame, current is B-frame, pts will be decreased
						fLast_fixed_pts =  fLast_fixed_pts + vop_time_increment - fLast_vop_time_increment;
						fLast_vop_coding_type = vop_coding_type;
						fLast_vop_time_increment = vop_time_increment;
					}
					else {
						//  the current frame break the boundary of vop_time_increment_resolution
						fLast_fixed_pts =  fLast_fixed_pts + vop_time_increment + vop_time_increment_resolution - fLast_vop_time_increment;
						fLast_vop_coding_type = vop_coding_type;
						fLast_vop_time_increment = vop_time_increment;
					}
				}

                //  check frame missing
				if (vop_coding_type != 2) {
					if (fLastNonBFrameInc > fixed_vop_time_increment) {
						fFrameMissing = (int)((double)fLastNonBFrameInc / (double)fixed_vop_time_increment - 0.5);
					}
					else {
						fFrameMissing = 0;
					}
					fLastNonBFrameInc = fLast_fixed_pts - fLastNonBFrameVop_time_increment;
					if (fLastNonBFrameInc < 0) {
						fLastNonBFrameInc += vop_time_increment_resolution;
					}
					else if (fLastNonBFrameInc >  vop_time_increment_resolution) {
						fLastNonBFrameInc -= vop_time_increment_resolution;
					}
				
					fLastNonBFrameVop_time_increment = fLast_fixed_pts;
				}
				else {
					fLastNonBFrameInc -= fixed_vop_time_increment;
					fFrameMissing = 0;
				}
			}
		}
	}
}


//   check the vop_coded value of the mpeg4 frame, if 0, the frame is not coded and will be dropped
Boolean AVIDemux::checkVOPCoded(unsigned char *framedata, unsigned frameSize)
{
	char fProfileAndLevelIndication;
	fVop_coded = True;
	if (frameSize >= 4 && framedata[0] == 0 && framedata[1] == 0 && framedata[2] == 1) {
		unsigned i = 3;
		if (framedata[i] == 0xB0 || framedata[i] == 0x00) { // VISUAL_OBJECT_SEQUENCE_START_CODE
			// The next byte is the "profile_and_level_indication":
			if (frameSize >= 5) fProfileAndLevelIndication = framedata[4];

			// The start of this frame - up to the first GROUP_VOP_START_CODE
			// or VOP_START_CODE - is stream configuration information.  Save this:
			for (i = 7; i < frameSize; ++i) {
				if ((framedata[i] == 0xB3 /*GROUP_VOP_START_CODE*/ ||
					framedata[i] == 0xB6 /*VOP_START_CODE*/)
					&& framedata[i-1] == 1 && framedata[i-2] == 0 && framedata[i-3] == 0) {
						break; // The configuration information ends here
				}
			}

			fConfigBytes = framedata;
			fNumConfigBytes = frameSize;

			analyzeVOLHeader();
		}

		if (i < frameSize) {
			u_int8_t nextCode = framedata[i];

			if (nextCode == 0xB3 /*GROUP_VOP_START_CODE*/) {
				// Skip to the following VOP_START_CODE (if any):
				for (i += 4; i < frameSize; ++i) {
					if (framedata[i] == 0xB6 /*VOP_START_CODE*/
						&& framedata[i-1] == 1 && framedata[i-2] == 0 && framedata[i-3] == 0) {
							nextCode = framedata[i];
							break;
					}
				}
			}

			if (nextCode == 0xB6 /*VOP_START_CODE*/ && i+3 <= frameSize) {
				++i;

				// Get the "vop_coding_type" from the next byte:
				u_int8_t nextByte = framedata[i++];
				u_int8_t vop_coding_type = nextByte>>6;

				// Next, get the "modulo_time_base" by counting the '1' bits that
				// follow.  We look at the next 32-bits only.
				// This should be enough in most cases.

				int fetchBytes = (frameSize - i) > 4 ? 4 : (frameSize - i);
				u_int32_t next4Bytes = 0;
				for (int j = 0; j < fetchBytes; j++) {
					next4Bytes |= framedata[i + j] << (24 - j * 8);
				}

			//	u_int32_t next4Bytes
			//		= (framedata[i]<<24)|(framedata[i+1]<<16)|(framedata[i+2]<<8)|framedata[i+3];
			//	i += 4;
				u_int32_t timeInfo = (nextByte<<(32-6))|(next4Bytes>>6);
				unsigned modulo_time_base = 0;
				u_int32_t mask = 0x80000000;
				while ((timeInfo&mask) != 0) {
					++modulo_time_base;
					mask >>= 1;
				}
				mask >>= 2;

				// Then, get the "vop_time_increment".
				unsigned vop_time_increment = 0;
				// First, make sure we have enough bits left for this:
				if ((mask>>(fNumVTIRBits-1)) != 0) {
					for (unsigned i = 0; i < fNumVTIRBits; ++i) {
						vop_time_increment |= timeInfo&mask;
						mask >>= 1;
					}
					
					fVop_coded = timeInfo&(mask>>1); 
					while (mask != 0) {
						vop_time_increment >>= 1;
						mask >>= 1;
					}
				}
			}
		}
	}
	
	if (fVop_coded) 
		return True;
	else 
		return False;
}

Boolean AVIDemux::getNextFrameBit(u_int8_t& result) {
	if (fNumBitsSeenSoFar/8 >= fNumConfigBytes) return False;

	u_int8_t nextByte = fConfigBytes[fNumBitsSeenSoFar/8];
	result = (nextByte>>(7-fNumBitsSeenSoFar%8))&1;
	++fNumBitsSeenSoFar;
	return True;
}

Boolean AVIDemux::getNextFrameBits(unsigned numBits,
								   u_int32_t& result) 
{
	result = 0;
	for (unsigned i = 0; i < numBits; ++i) {
		u_int8_t nextBit;
		if (!getNextFrameBit(nextBit)) return False;
		result = (result<<1)|nextBit;
	}
	return True;
}

void AVIDemux::analyzeVOLHeader() {
	// Begin by moving to the VOL header:
	if (fVOPHeaderParsed == True) {
		return;
	}
	
	unsigned i;
	for (i = 3; i < fNumConfigBytes; ++i) {
		if (fConfigBytes[i] >= 0x20 && fConfigBytes[i] <= 0x2F
				&& fConfigBytes[i-1] == 1
				&& fConfigBytes[i-2] == 0 && fConfigBytes[i-3] == 0) {
			++i;
			break;
		}
	}

	fNumBitsSeenSoFar = 8*i + 9;

	do {
		u_int8_t is_object_layer_identifier;
		if (!getNextFrameBit(is_object_layer_identifier)) break;
		if (is_object_layer_identifier) fNumBitsSeenSoFar += 7;

		u_int32_t aspect_ratio_info;
		if (!getNextFrameBits(4, aspect_ratio_info)) break;
		if (aspect_ratio_info == 15 /*extended_PAR*/) fNumBitsSeenSoFar += 16;

		u_int8_t vol_control_parameters;
		if (!getNextFrameBit(vol_control_parameters)) break;
		if (vol_control_parameters) {
			fNumBitsSeenSoFar += 3; // chroma_format; low_delay
			u_int8_t vbw_parameters;
			if (!getNextFrameBit(vbw_parameters)) break;
			if (vbw_parameters) fNumBitsSeenSoFar += 79;
		}

		fNumBitsSeenSoFar += 2; // video_object_layer_shape
		u_int8_t marker_bit;
		if (!getNextFrameBit(marker_bit)) break;
		if (marker_bit != 1) break; // sanity check

		if (!getNextFrameBits(16, vop_time_increment_resolution)) break;
		if (vop_time_increment_resolution == 0) break; // shouldn't happen

		fixed_vop_time_increment = (int)((double)vop_time_increment_resolution * (double)fMicroSecPerFrame / 1000000.0 + 0.5);
			
		// Compute how many bits are necessary to represent this:
		fNumVTIRBits = 0;
		for (unsigned test = vop_time_increment_resolution; test>0; test /= 2) {
			++fNumVTIRBits;
		}
		
		if (!getNextFrameBit(marker_bit)) break;
	    if (marker_bit != 1) { // sanity check
			break;
	    }

	    if (!getNextFrameBit(fixed_vop_rate)) break;
	    if (fixed_vop_rate) {
	        // Get the following "fixed_vop_time_increment":
	        if (!getNextFrameBits(fNumVTIRBits, fixed_vop_time_increment)) break;
	    }	
		fVOPHeaderParsed = True;
	} while (0);
}

