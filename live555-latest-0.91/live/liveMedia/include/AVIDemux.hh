#ifndef _AVI_DEMUX_HH
#define _AVI_DEMUX_HH

#ifndef _FRAMED_SOURCE_HH
#include "FramedSource.hh"
#endif

extern "C" {
#ifndef UINT64_C
#define UINT64_C(v) uint64_t(v ## ll)  
#endif
#ifndef INT64_C
#define INT64_C(v) int64_t(v ## ll)  
#endif
#include "libavutil/avutil.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
};

class AVIDemuxedElementaryStream; // forward

#define MAX_AVI_STREAM 2    //  one for video, one for audio

class AVIDemux: public Medium {
public:
	static AVIDemux* createNew(UsageEnvironment& env,
		char const* inputFileName);
	

	AVIDemuxedElementaryStream* newElementaryStream(char streamIndex);

	// Specialized versions of the above for audio and video:
	AVIDemuxedElementaryStream* newAudioStream();
	AVIDemuxedElementaryStream* newVideoStream();

	void getNextFrame(char streamIndex,
		unsigned char* to, unsigned maxSize,
		FramedSource::afterGettingFunc* afterGettingFunc,
		void* afterGettingClientData,
		FramedSource::onCloseFunc* onCloseFunc,
		void* onCloseClientData);
	// similar to FramedSource::getNextFrame(), except that it also
	// takes a stream id tag as parameter.

	void stopGettingFrames(char streamIndex);
	// similar to FramedSource::stopGettingFrames(), except that it also
	// takes a stream id tag as parameter.

	void flushInput(); // should be called before any 'seek' on the underlying source

	int prepare();    //  parse the AVI file header, return 0 if the file is a valid .AVI video, otherwise return -1;

	int seekStream(float pos);
	float getDuration();      //  get AVI file duration, format: [sec].[usec]
	u_int32_t getFrameTime(); //  get the duration of one frame in Microsecond 

	u_int32_t getCodecID(char streamIndex);   //  return the codec ID of A/V stream
	u_int32_t getSamplingRate();				  //  return the audio sampling rate
	void getConfigData(char streamIndex, uint8_t **data, int *size);  //  return the A/V codec config data

	char getVideoIndex() { return fVideoIndex; };
	char getAudioIndex() { return fAudioIndex; };
	unsigned int getBitrate(char streamIndex);  //  return bitrate of the stream int kbps
	unsigned int getNumChannels();              //  return audio channels
	
private:
	AVIDemux(UsageEnvironment& env,	char const* inputFileName);

	// called only by createNew()
	virtual ~AVIDemux();

	void registerReadInterest(char streamIndex,
		unsigned char* to, unsigned maxSize,
		FramedSource::afterGettingFunc* afterGettingFunc,
		void* afterGettingClientData,
		FramedSource::onCloseFunc* onCloseFunc,
		void* onCloseClientData);

	Boolean useSavedData(char streamIndex,
		unsigned char* to, unsigned maxSize,
		FramedSource::afterGettingFunc* afterGettingFunc,
		void* afterGettingClientData);

	void continueReadProcessing(char streamIndex,
		unsigned char* to, unsigned maxSize,
		FramedSource::afterGettingFunc* afterGettingFunc,
		void* afterGettingClientData,
		FramedSource::onCloseFunc* onCloseFunc,
		void* onCloseClientData);
	
	static void continueReadProcessing(void* clientData);
	void continueReadProcessing();

	void fixMPEG4PTS(unsigned char *framedata, unsigned frameSize);

	Boolean getNextFrameBit(u_int8_t& result);
	Boolean getNextFrameBits(unsigned numBits, u_int32_t& result);
	// Which are used by:
	void analyzeVOLHeader();

	void sendVideoFrame(unsigned char* data, unsigned size);
	int findVOPStartCode(unsigned char* data, int start_pos, int size);

	int updateUserData(unsigned char* data, unsigned size);

	Boolean checkVOPCoded(unsigned char *framedata, unsigned frameSize);

	void GetFrame(AVPacket *packet);

	//  check if the packet is key frame
	Boolean isKeyFrame(AVPacket *pkt);
	
	//  convert ffmpeg time to double 
	double convertPTS(AVPacket *pkt);
private:
	friend class AVIDemuxedElementaryStream;
	void noteElementaryStreamDeletion(AVIDemuxedElementaryStream* es);


private:
	// A descriptor for each possible stream id tag:
	typedef struct OutputDescriptor {
		// input parameters
		unsigned char* to; unsigned maxSize;
		FramedSource::afterGettingFunc* fAfterGettingFunc;
		void* afterGettingClientData;
		FramedSource::onCloseFunc* fOnCloseFunc;
		void* onCloseClientData;

		// output parameters
		unsigned frameSize; 
		struct timeval presentationTime;
		class SavedData; // forward
		SavedData* savedDataHead;
		SavedData* savedDataTail;
		unsigned savedDataTotalSize;

		// status parameters
		Boolean isPotentiallyReadable;
		Boolean isCurrentlyActive;
		Boolean isCurrentlyAwaitingData;
	} OutputDescriptor_t;
	
	//  one for Video, one  for Audio
	OutputDescriptor_t fOutput[MAX_AVI_STREAM];
	int fVideoIndex;
	int fAudioIndex;
	
	char * fInputFileName;

	//  microseconds per one video frame
	int fMicroSecPerFrame;

	AVFormatContext *fFormatCtx;

	//  for MPEG4 header analyzing
	unsigned fNumBitsSeenSoFar; // used by the getNextFrameBit*() routines
	u_int32_t vop_time_increment_resolution;
	unsigned fNumVTIRBits;      // # of bits needed to count to "vop_time_increment_resolution"
	u_int8_t fixed_vop_rate;
	unsigned fixed_vop_time_increment; // used if 'fixed_vop_rate' is set
	u_int8_t *fConfigBytes;
	unsigned fNumConfigBytes;
	uint64_t fLast_fixed_pts;
	unsigned fLast_vop_time_increment;
	unsigned fLast_vop_coding_type;
	unsigned fVop_coded;
	Boolean  fFirst_vop;

	Boolean fVOPHeaderParsed;

	Boolean fUseH264BSF;
	AVBitStreamFilterContext* fH264BSF;
	//  for A/V Synchronize after Seek
	double fVideo_diff;

	//  some AVI have 0-size frames, the pts is not continual
	uint64_t fLastNonBFrameVop_time_increment;
	uint64_t fLastNonBFrameInc;
	int fFrameMissing;

	friend class GenericStreamer; //  hack
};

#endif
