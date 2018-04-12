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
// A 'ServerMediaSubsession' object that creates new, unicast, "RTPSink"s
// on demand, from a AVI demuxer.
// Implementation

#include "AVIDemuxedServerMediaSubsession.hh"
#include "MPEG1or2AudioStreamFramer.hh"
#include "MPEG1or2AudioRTPSink.hh"
#include "MPEG1or2VideoStreamFramer.hh"
#include "MPEG1or2VideoRTPSink.hh"
#include "AC3AudioStreamFramer.hh"
#include "AC3AudioRTPSink.hh"
#include "ByteStreamFileSource.hh"
#include "MPEG4VideoStreamFramer.hh"
#include "MPEG4VideoStreamDiscreteFramer.hh"
#include "MPEG4ESVideoRTPSink.hh"
#include "AVIVideoDiscreteFramer.hh"
#include "AACAudioStreamFramer.hh"
#include "MPEG4GenericRTPSink.hh"
#include "H264VideoStreamFramer.hh"
#include "H264VideoRTPSink.hh"

AVIDemuxedServerMediaSubsession* AVIDemuxedServerMediaSubsession
::createNew(AVIFileServerDemux& demux, char streamIndex,
            Boolean reuseFirstSource, Boolean iFramesOnly, double vshPeriod)
{
    return new AVIDemuxedServerMediaSubsession(demux, streamIndex,
            reuseFirstSource,
            iFramesOnly, vshPeriod);
}

AVIDemuxedServerMediaSubsession
::AVIDemuxedServerMediaSubsession(AVIFileServerDemux& demux,
                                  char streamIndex, Boolean reuseFirstSource,
                                  Boolean iFramesOnly, double vshPeriod)
    : OnDemandServerMediaSubsession(demux.envir(), reuseFirstSource),
      fOurDemux(demux), fStreamIndex(streamIndex),
      fIFramesOnly(iFramesOnly), fVSHPeriod(vshPeriod),
	  fVideoAuxSDPLine(NULL), fAudioAuxSDPLine(NULL)
{
}

AVIDemuxedServerMediaSubsession::~AVIDemuxedServerMediaSubsession()
{
	delete[] fVideoAuxSDPLine;
	delete[] fAudioAuxSDPLine;
}

FramedSource* AVIDemuxedServerMediaSubsession
::createNewStreamSource(unsigned clientSessionId, unsigned& estBitrate)
{
    FramedSource* es = NULL;
    do {
		//printf("#createNewStreamSource# ***\n");
        es = fOurDemux.newElementaryStream(clientSessionId, fStreamIndex);
		//printf("#createNewStreamSource# ***\n");
        if (es == NULL) break;

        if (fStreamIndex == fOurDemux.fSession0Demux->getAudioIndex() /*MPEG audio*/) {
            estBitrate = fOurDemux.fSession0Demux->getBitrate(fStreamIndex); // kbps
			uint32_t codec_id = fOurDemux.fSession0Demux->getCodecID(fStreamIndex);
			//printf("#createNewStreamSource# audio smss: 0x%x  demux: 0x%x\n", this, fOurDemux.fSession0Demux);
			if (codec_id == CODEC_ID_MP3 || codec_id == CODEC_ID_MP2) {
				return MPEG1or2AudioStreamFramer::createNew(envir(), es);
			}
			else if (codec_id == CODEC_ID_AC3) {
				return AC3AudioStreamFramer::createNew(envir(), es);
			}
			else if (codec_id == CODEC_ID_AAC) {
				return AACAudioStreamFramer::createNew(envir(), es);
			}
			else {
				envir()<< "Unsupported Audio Type \n"; 
			}
        } else if (fStreamIndex == fOurDemux.fSession0Demux->getVideoIndex() /*video*/) {
            estBitrate = fOurDemux.fSession0Demux->getBitrate(fStreamIndex); // kbps
			uint32_t codec_id = fOurDemux.fSession0Demux->getCodecID(fStreamIndex);
			//printf("#createNewStreamSource# video smss: 0x%x  demux: 0x%x\n", this, fOurDemux.fSession0Demux);
			if (codec_id == CODEC_ID_MPEG2VIDEO) {
				return MPEG1or2VideoStreamFramer::createNew(envir(), es, fIFramesOnly, fVSHPeriod);
			}
			else if (codec_id == CODEC_ID_MPEG4) {
				return AVIVideoDiscreteFramer::createNew(envir(), es);
			}
			else if (codec_id == CODEC_ID_H264) {
				return H264VideoStreamFramer::createNew(envir(), es);
			}
        } else { // unknown stream type
            break;
        }
    } while (0);

    // An error occurred:
    Medium::close(es);
    return NULL;
}

RTPSink* AVIDemuxedServerMediaSubsession
::createNewRTPSink(Groupsock* rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic,
                   FramedSource* inputSource)
{
    if (fStreamIndex == fOurDemux.fSession0Demux->getAudioIndex() /*MPEG audio*/) {
		uint32_t codec_id = fOurDemux.fSession0Demux->getCodecID(fStreamIndex);
		if (codec_id == CODEC_ID_MP3 || codec_id == CODEC_ID_MP2) {
			return MPEG1or2AudioRTPSink::createNew(envir(), rtpGroupsock);
		}
		else if (codec_id == CODEC_ID_AC3) {
			return AC3AudioRTPSink::createNew(envir(), rtpGroupsock, 97, fOurDemux.fSession0Demux->getSamplingRate());
		}
		else if (codec_id == CODEC_ID_AAC ) {
			uint8_t *data;
			int size;
			fOurDemux.fSession0Demux->getConfigData(fStreamIndex, &data, &size);
			char* fmtp = new char[size * 2 + 1];
			char* ptr = fmtp;
			for (int i = 0; i < size; ++i) {
				sprintf(ptr, "%02X", data[i]);
				ptr += 2;
			}

			return MPEG4GenericRTPSink::createNew(envir(), 
												  rtpGroupsock, 97, 
												  fOurDemux.fSession0Demux->getSamplingRate(),
												  "audio", "AAC-hbr",
												  fmtp,
												  fOurDemux.fSession0Demux->getNumChannels());
		}
		else {
			envir()<< "Unsupported Audio Type \n"; 
			return NULL;
		}
    } else if ((fStreamIndex) == fOurDemux.fSession0Demux->getVideoIndex() /*video*/) {
		uint32_t codec_id = fOurDemux.fSession0Demux->getCodecID(fStreamIndex);
		if (codec_id == CODEC_ID_MPEG2VIDEO) {
			return MPEG1or2VideoRTPSink::createNew(envir(), rtpGroupsock);
		}
		else if (codec_id == CODEC_ID_MPEG4) {
        	return MPEG4ESVideoRTPSink::createNew(envir(), rtpGroupsock, 96, 90000);
		}
		else if(codec_id == CODEC_ID_H264) {
			return H264VideoRTPSink::createNew(envir(), rtpGroupsock, rtpPayloadTypeIfDynamic);
		}
    }

    return NULL;
}

void AVIDemuxedServerMediaSubsession
::seekStreamSource(FramedSource* /*inputSource*/, double& seekNPT, 
				double /*streamDuration*/, u_int64_t& /*numBytes*/)
{
    float const dur = duration();
    unsigned const size = fOurDemux.fileSize();
    unsigned absBytePosition = dur == 0.0 ? 0 : (unsigned)((seekNPT/dur)*size);
	printf("#seekStreamSource# smss: 0x%x\n",this);
	fOurDemux.fSession0Demux->seekStream((float)seekNPT);
	
	//  Mark.ma -- Seek function not implemented yet, TBD 
	return ;
}

float AVIDemuxedServerMediaSubsession::duration() const
{
    return fOurDemux.fileDuration();
}

//  generate auxSDP info 
char const* AVIDemuxedServerMediaSubsession::getAuxSDPLine(RTPSink* rtpSink, FramedSource* inputSource)
{
	if (fStreamIndex < 0) {
		return NULL;
	}

	if (fStreamIndex == fOurDemux.fSession0Demux->getAudioIndex() /*MPEG audio*/) {
		return rtpSink == NULL ? NULL : rtpSink->auxSDPLine();
	} else if (fStreamIndex == fOurDemux.fSession0Demux->getVideoIndex() /*video*/) {
		int microSecPerFrame = fOurDemux.fSession0Demux->getFrameTime();
		uint8_t *data;
		int size;
		fOurDemux.fSession0Demux->getConfigData(fStreamIndex, &data, &size);
		
		char const* fmtpFmt =
			"a=framerate: %3.3f\r\n"
			"a=fmtp:%d "
			"config=";
		unsigned fmtpFmtSize = strlen(fmtpFmt)
			+ 7 /* max char len */
			+ 3 /* max char len */
			+ 2*size /* 2*, because each byte prints as 2 chars */
			+ 2 /* trailing \r\n */;

		char* fmtp = new char[fmtpFmtSize];
		sprintf(fmtp, fmtpFmt, 1000000.0 / microSecPerFrame, 96);

		char* endPtr = &fmtp[strlen(fmtp)];
		for (int i = 0; i < size; ++i) {
			sprintf(endPtr, "%02X", data[i]);
			endPtr += 2;
		}
		sprintf(endPtr, "\r\n");
		
		delete[] fVideoAuxSDPLine;
		fVideoAuxSDPLine = strDup(fmtp);
		delete[] fmtp;
		return fVideoAuxSDPLine;
	} 

	return NULL;
}

