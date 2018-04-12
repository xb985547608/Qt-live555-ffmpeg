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
// C++ header

#ifndef _AVI_DEMUXED_SERVER_MEDIA_SUBSESSION_HH
#define _AVI_DEMUXED_SERVER_MEDIA_SUBSESSION_HH

#ifndef _ON_DEMAND_SERVER_MEDIA_SUBSESSION_HH
#include "OnDemandServerMediaSubsession.hh"
#endif
#ifndef _AVI_FILE_SERVER_DEMUX_HH
#include "AVIFileServerDemux.hh"
#endif

class AVIDemuxedServerMediaSubsession: public OnDemandServerMediaSubsession{
public:
	static AVIDemuxedServerMediaSubsession*
		createNew(AVIFileServerDemux& demux, char streamIndex,
		Boolean reuseFirstSource,
		Boolean iFramesOnly = False, double vshPeriod = 5.0);
	// The last two parameters are relevant for video streams only

private:
	AVIDemuxedServerMediaSubsession(AVIFileServerDemux& demux,
		char streamIndex, Boolean reuseFirstSource,
		Boolean iFramesOnly, double vshPeriod);
	// called only by createNew();
	virtual ~AVIDemuxedServerMediaSubsession();

private: // redefined virtual functions
	virtual void seekStreamSource(FramedSource* inputSource, double& seekNPT, double streamDuration, u_int64_t& numBytes);
	virtual FramedSource* createNewStreamSource(unsigned clientSessionId,
		unsigned& estBitrate);
	virtual RTPSink* createNewRTPSink(Groupsock* rtpGroupsock,
		unsigned char rtpPayloadTypeIfDynamic,
		FramedSource* inputSource);
	virtual float duration() const;
	virtual char const* getAuxSDPLine(RTPSink* rtpSink, FramedSource* inputSource);

private:
	AVIFileServerDemux& fOurDemux;
	char fStreamIndex;
	Boolean fIFramesOnly; // for video streams
	double fVSHPeriod; // for video streams

	char* fVideoAuxSDPLine;
	char* fAudioAuxSDPLine;
};

#endif
