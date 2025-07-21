//  Mach1 Spatial SDK
//  Copyright © 2017-2021 Mach1. All rights reserved.

/*
 Order of Operations:
 1. Setup Input and Output formats (and paths)
 2. Call `processConversionPath()` to setup the conversion for processing
 3. Use `setSpatialDownmixer()` & `getSpatialDownmixerPossibility()` to downmix content to Mach1Horizon if top/bottom
    difference is less than correlation threshold
    Note: Afterwards reinitizalize setup of Input and Output formats
 4. Call `processConversion()` to execute the conversion and return coeffs per buffer/sample per channel
 5. Apply to buffer/samples per channel in file rendering or audio mixer
 */

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdlib.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <sstream>
#include <fstream>
#include <string>

#include "Mach1Transcode.h"
#include "Mach1AudioTimeline.h"

#include "sndfile.hh"
#include "CmdOption.h"
#include "ADMParse.h"
#include "yaml/Yaml.hpp"
#include "xml/pugixml.hpp"
#include "bw64/bw64.hpp"
#include "adm_metadata.h"

std::vector<Mach1AudioObject> audioObjects;
std::vector<Mach1Point3D> keypoints;

Mach1Point3D* callbackPointsSampler(long long sample, int& n) {
    keypoints.resize(audioObjects.size());
    
    for (int i = 0; i < audioObjects.size(); i++) {
        std::vector<Mach1KeyPoint> kp = audioObjects[i].getKeyPoints();

        keypoints[i] = kp[0].point;
        for (int j = 0; j < kp.size(); j++) {
            if (kp[j].sample >= sample) {
                keypoints[i] = kp[j].point;
                continue;
            }
        }
    }
    n = keypoints.size();
    return keypoints.data();
}

using namespace std;

vector<string> &split(const string &s, char delim, vector<string> &elems) {
	stringstream ss(s);
	string item;
	while (getline(ss, item, delim)) {
		elems.push_back(item);
	}
	return elems;
}

string convertToString(char* a, int size) {
	int i;
	string s = "";
	for (i = 0; i < size; i++) {
		s = s + a[i];
	}
	return s;
}

void printHelp() {
	std::cout << "m1-transcode -- command line mach1 format conversion tool" << std::endl;
	std::cout << "ambisonics in collaboration with VVAudio: http://www.vvaudio.com/ " << std::endl;
	std::cout << std::endl;
	std::cout << "usage: ./m1-transcode -in-file test_s8.wav -in-fmt M1Spatial -out-file test_b.wav -out-fmt ACNSN3D -out-file-chans 0" << std::endl;
    std::cout << "usage: ./m1-transcode -in-file test_s8.wav -in-fmt M1Spatial -out-file 7_1_2-ADM.wav -out-fmt 7.1.2_M -write-metada -out-file-chans 0" << std::endl;
    std::cout << std::endl;
    std::cout << "all boolean argument flags should be used before the end of the command to ensure it is captured" << std::endl;
	std::cout << std::endl;
	std::cout << "  -help                 - list command line options" << std::endl;
    std::cout << "  -formats              - list all available formats" << std::endl;
	std::cout << "  -in-file  <filename>  - input file: put quotes around sets of files" << std::endl;
	std::cout << "  -in-fmt   <fmt>       - input format: see supported formats below" << std::endl;
    std::cout << "  -in-json  <json>      - input json: for input custom json Mach1Transcode templates" << std::endl;
	std::cout << "  -out-file <filename>  - output file. full name for single file or name stem for file sets" << std::endl;
	std::cout << "  -out-fmt  <fmt>       - output format: see supported formats below" << std::endl;
    std::cout << "  -out-json  <json>     - output json: for output custom json Mach1Transcode templates" << std::endl;
	std::cout << "  -out-file-chans <#>   - output file channels: 1, 2 or 0 (0 = multichannel)" << std::endl;
	std::cout << "  -normalize            - two pass normalize absolute peak to zero dBFS" << std::endl;
	std::cout << "  -master-gain <#>      - final output gain in dB like -3 or 2.3" << std::endl;
	std::cout << "  -lfe-sub <#>          - indicates channel(s) to be filtered and treated as LFE/SUB, delimited by ',' for multiple channels" << std::endl;
	std::cout << "  -spatial-downmix <#>  - compare top vs. bottom of the input soundfield, if difference is less than the set threshold (float) output format will be Mach1 Horizon" << std::endl;
	std::cout << "  -extract-metadata     - export any detected XML metadata into separate text file" << std::endl;
	std::cout << "  -write-metadata       - write channel-bed ADM metadata for supported formats" << std::endl;
	std::cout << std::endl;
}

void printFormats() {
    Mach1Transcode<float> formatLister;
    formatLister.setInputFormat(formatLister.getFormatFromString("1.0"));
    formatLister.setOutputFormat(formatLister.getFormatFromString("M1Spatial-8"));
    formatLister.processConversionPath();
    std::vector<std::vector<float>> matrix = formatLister.getMatrixConversion();
    std::vector<std::string> formats = formatLister.getAllFormatNames();
    
    std::cout << "  Format Descriptions:" << std::endl;
    std::cout << "    - M or Music          = `Music Mix` (Channels are spaced out evenly throughout the horizontal soundfield)" << std::endl;
    std::cout << "    - C or Cinema         = `Cinema Mix` (Channels are more focused on the front)" << std::endl;
    std::cout << "    - S or SideSurround   = `Side Surround Mix` (Surround channels are oriented more to the sides instead of rear (+-110 azimuth instead of +-135))" << std::endl;
    std::cout << "    - R or RearSurround   = `Rear Surround Mix` (Surround channels are oriented more to the rears instead of sides (+-154 azimuth instead of +-135))" << std::endl;
    std::cout << "    - SIM or Simulated    = `Simulated Room Mix` (Lessens the divergence of virtual speakers to quickly simulate hearing front/back soundfield within a real world listening environment)" << std::endl;
    std::cout << std::endl;
    std::cout << "  Mach1 Spatial Best Practices:" << std::endl;
    std::cout << "    - C / S / R surround configurations should use Mach1Spatial-12 as a minimum to correctly handle the transcoding of a dedicated Center channel" << std::endl;
    std::cout << "    - M or SIM surround configurations could be retained within lower Mach1Spatial-4 / Mach1Spatial-8 containers" << std::endl;
    std::cout << std::endl;
    std::cout << "  Formats Supported:" << std::endl;
    for (auto fmt = 0; fmt < formats.size(); fmt++) {
        std::cout << "    " << formats[fmt] << std::endl;
    }
    std::cout << std::endl;
}

struct audiofileInfo {
    int format;
    int sampleRate;
    int numberOfChannels;
    float duration;
};

audiofileInfo printFileInfo(SndfileHandle file, bool displayLength = true) {
    audiofileInfo inputFileInfo;
    std::cout << "Sample Rate:        " << file.samplerate() << std::endl;
    inputFileInfo.sampleRate = file.samplerate();
    int format = file.format() & 0xffff;
    if (format == SF_FORMAT_PCM_16) std::cout << "Bit Depth:          16" << std::endl;
    if (format == SF_FORMAT_PCM_24) std::cout << "Bit Depth:          24" << std::endl;
    if (format == SF_FORMAT_PCM_32) std::cout << "Bit Depth:          32" << std::endl;
    inputFileInfo.format = format;
    std::cout << "Channels:           " << file.channels() << std::endl;
    inputFileInfo.numberOfChannels = file.channels();
    
    if (displayLength) {
        std::cout << "Length (sec):       " << (float)file.frames() / (float)file.samplerate() << std::endl;
    }
    inputFileInfo.duration = (float)file.frames() / (float)file.samplerate();
    
    if (file.getString(SF_STR_SOFTWARE) != NULL) {
        std::cout << "Software:           " << file.getString(SF_STR_SOFTWARE) << std::endl;
    }
    
    if (file.getString(SF_STR_COMMENT) != NULL) {
        std::cout << "Comment:            " << file.getString(SF_STR_COMMENT) << std::endl;
    }
    
    std::cout << std::endl;
    
    return inputFileInfo;
}

std::string prepareAdmMetadata(const char* admString, float duration, int sampleRate, int format) {
    // Used to find duration in time of input file
    // to correctly edit the ADM metadata and add the appropriate
    // `end` and `duration` times.
    std::string s(admString);
    std::string searchDurationString("hh:mm:ss.fffff");
    size_t pos = s.find(searchDurationString);
    
    int seconds, minutes, hours;
    std::string hoursString, minutesString, secondsString;
    seconds = duration;
    minutes = seconds / 60;
    hours = minutes / 60;
    
    if ((int)hours < 100) {
        hoursString = (int(hours) < 10) ? "0" + std::to_string(int(hours)) : std::to_string(int(hours));
    } else {
        // file duration too long?
        // TODO: handle case for when input is over 99 hours
    }
    minutesString = (int(minutes%60) < 10) ? "0" + std::to_string(int(minutes%60)) : std::to_string(int(minutes%60));
    secondsString = (int((seconds+1)%60) < 10) ? "0" + std::to_string(int((seconds+1)%60)) : std::to_string(int((seconds+1)%60));
    
    std::vector<size_t> positions;
    // Repeat till end is reached
    while(pos != std::string::npos){
        // Add position to the vector
        positions.push_back(pos);
        // Get the next occurrence from the current position
        pos = s.find(searchDurationString, pos + searchDurationString.size());
    }

    for (size_t pos : positions){
        if (pos != s.npos){
            s.replace(pos, searchDurationString.length(), hoursString+":"+minutesString+":"+secondsString+".00000");
        }
    }

    std::cout << "Detected Duration:  " << duration << std::endl;
    std::cout << "Duration Timecode:  " << hoursString << ":" << minutesString << ":" << secondsString << ".00000" << std::endl;
    
    // set metadata for samplerate
    std::string searchSampleRateString("__SAMPLERATE__");
    size_t srPos = s.find(searchSampleRateString);
    // Repeat till end is reached
    while(srPos != std::string::npos){
        if (srPos != s.npos) {
            s.replace(srPos, searchDurationString.length(), std::to_string(sampleRate));
        }
        // Get the next occurrence from the current position
        srPos = s.find(searchSampleRateString, srPos + searchSampleRateString.size());
    }
    std::cout << "Detected SampleRate:  " << std::to_string(sampleRate) << std::endl;
    
    // set metadata for bitdepth
    std::string searchBitDepthString("__BITDEPTH__");
    size_t bdPos = s.find(searchBitDepthString);
    // Repeat till end is reached
    while(bdPos != std::string::npos){
        if (bdPos != s.npos) {
            s.replace(bdPos, searchBitDepthString.length(), std::to_string(format));
        }
        // Get the next occurrence from the current position
        bdPos = s.find(searchBitDepthString, bdPos + searchBitDepthString.size());
    }
    std::cout << "Detected BitDepth:  " << std::to_string(format) << std::endl;
    
    return s;
}

// ---------------------------------------------------------
#define BUFFERLEN 512

class SndFileWriter {
    std::unique_ptr<bw64::Bw64Writer> outBw64;
    SndfileHandle outSnd;
    int channels;
    
    enum SNDFILETYPE {
        SNDFILETYPE_BW64,
        SNDFILETYPE_SND
    } type;

public:
    void open(std::string outfilestr, int sampleRate, int channels, int format) {
        outSnd = SndfileHandle(outfilestr, SFM_WRITE, format, channels, (int)sampleRate);
        this->channels = channels;
        type = SNDFILETYPE_SND;
    }

    void open(std::string outfilestr, int sampleRate, int channels, int format, bw64::ChnaChunk chnaChunkAdm, bw64::AxmlChunk axmlChunkAdm) {
        // TODO: make variable of samplerate and bitdepth based on input
        outBw64 = bw64::writeFile(outfilestr, channels, sampleRate, format, std::make_shared<bw64::ChnaChunk>(chnaChunkAdm), std::make_shared<bw64::AxmlChunk>(axmlChunkAdm));
        this->channels = channels;
        type = SNDFILETYPE_BW64;
    }

    bool isOpened() {
        if (type == SNDFILETYPE_SND) {
            return outSnd.error() == 0;
        } else {
            return true;
        }
    }

    void setClip() {
        if (type == SNDFILETYPE_SND) {
            outSnd.command(SFC_SET_CLIPPING, NULL, SF_TRUE);
        }
    }

    void printInfo() {
        if (type == SNDFILETYPE_SND) {
            printFileInfo(outSnd, false);
        }
    }

    void setString(int str_type, const char* str) {
        if (type == SNDFILETYPE_SND) {
            outSnd.setString(str_type, str);
        }
    }

    void write(float* buf, int frames) {
        if (type == SNDFILETYPE_SND) {
            outSnd.write(buf, frames*channels);
        } else {
            outBw64->write(buf, frames);
            outBw64->framesWritten();
        }
    }
};

void parseFile(SndfileHandle infile, int channels) {
	float buf[BUFFERLEN];
	sf_count_t frames;
	int k, m, readcount;

	frames = BUFFERLEN / channels;
	while ((readcount = infile.readRaw(buf, frames)) > 0) {
		for (k = 0; k < readcount; k++) {
			for (m = 0; m < channels; m++) {
				// search here for xml
				//std::cout << "Buffer: " << buf [k * channels + m];
				//std::cout << std::endl;
			}
		}
	};
}

int main(int argc, char* argv[]) {
    Mach1AudioTimeline m1audioTimeline;
    Mach1Transcode<float> m1transcode;
    m1transcode.setCustomPointsSamplerCallback(callbackPointsSampler);

	// locals for cmd line parameters
	bool fileOut = false;
    bool useAudioTimeline = false; // adm, atmos formats
	//TODO: inputGain = 1.0f; // in level, not db
	float masterGain = 1.0f; // in level, not dB
	bool normalize = false;
    char* infolder = NULL;
	char* infilename = NULL;
	char* inFmtStr = NULL;
	int inFmt;
	char* outfilename = NULL;
	std::string md_outfilename = "";
	char* outFmtStr = NULL;
    int outFmt;
	int outFileChans;
	int channels;
	bool spatialDownmixerMode = false;
	float corrThreshold = 0.0;
	std::vector<int> subChannelIndices;
	bool processSubs = false;
	bool extractMetadata = false;
    bool writeMetadata = false;
	ADMParse admParse; // Reading ADM data

	sf_count_t totalSamples;
	long sampleRate;

	// multiplexed buffers
	float fileBuffer[Mach1TranscodeMAXCHANS * BUFFERLEN];

	// process buffers
	float inBuffers[Mach1TranscodeMAXCHANS][BUFFERLEN];
	float *inPtrs[Mach1TranscodeMAXCHANS];
	float outBuffers[Mach1TranscodeMAXCHANS][BUFFERLEN];
	float *outPtrs[Mach1TranscodeMAXCHANS];
	for (int i = 0; i < Mach1TranscodeMAXCHANS; i++) {
		inPtrs[i] = inBuffers[i];
		outPtrs[i] = outBuffers[i];
	}

	//=================================================================
	// read command line parameters
	//

	char *pStr;
	if (cmdOptionExists(argv, argv + argc, "-h")
		|| cmdOptionExists(argv, argv + argc, "-help")
		|| cmdOptionExists(argv, argv + argc, "--help")
		|| argc == 1)
	{
		printHelp();
		return 0;
	}
    if (cmdOptionExists(argv, argv + argc, "-f")
        || cmdOptionExists(argv, argv + argc, "-formats")
        || cmdOptionExists(argv, argv + argc, "-format-list")
        || cmdOptionExists(argv, argv + argc, "--formats")
        || argc == 1)
    {
        printFormats();
        return 0;
    }
	pStr = getCmdOption(argv, argv + argc, "-normalize");
	if (pStr != NULL)
	{
		normalize = true;
	}
	pStr = getCmdOption(argv, argv + argc, "-master-gain");
	if (pStr != NULL)
	{
		masterGain = (float)atof(pStr); // still in dB
		masterGain = m1transcode.db2level(masterGain);
	}
	pStr = getCmdOption(argv, argv + argc, "-lfe-sub");
	/*
	 Submit channel index int(s) with commas as delimiters
	 Example: -lfe-sub 3,7
	 Indicates channels 4 and 8 as sub/LFE channels
	 */
	if (pStr && (strlen(pStr) > 0))
	{
		processSubs = true;
		char* lfeIndices = strtok(pStr, ",");
		while (lfeIndices) {
			//pushback int safe only
			subChannelIndices.push_back(stoi(lfeIndices));
			lfeIndices = strtok(NULL, ",");
		}
	}
	// flag for extracting metadata to a separate text file
	pStr = getCmdOption(argv, argv + argc, "-extract-metadata");
	if (pStr != NULL)
	{
		extractMetadata = true;
	}
    // flag for writing ADM metadata to audiofile if supported
    pStr = getCmdOption(argv, argv + argc, "-write-metadata");
    if (pStr != NULL)
    {
        writeMetadata = true;
    }
	/*
	 flag for auto Mach1 Spatial downmixer
	 compares top/bottom to downmix to Horizon
	 TODO: scale to other formats
	 */
	pStr = getCmdOption(argv, argv + argc, "-spatial-downmix");
	if (pStr != NULL)
	{
		spatialDownmixerMode = true;
		corrThreshold = atof(pStr);
	}
	if (spatialDownmixerMode && (corrThreshold < 0.0 || corrThreshold > 1.0))
	{
		std::cout << "Please use 0.0 to 1.0 range for correlation threshold" << std::endl;
		return -1;
	}
	// input file name and format
	pStr = getCmdOption(argv, argv + argc, "-in-file");
	if (pStr && (strlen(pStr) > 0))
	{
		infilename = pStr;
	}
	else
	{
		cerr << "Please specify an input file" << std::endl;
		return -1;
	}
	pStr = getCmdOption(argv, argv + argc, "-in-fmt");
	if (pStr && (strlen(pStr) > 0))
    {
		inFmtStr = pStr;
        
        if (strcmp(inFmtStr, "ADM") == 0) {
            m1transcode.setInputFormat(m1transcode.getFormatFromString("CustomPoints"));
            m1audioTimeline.parseADM(infilename);
            useAudioTimeline = true;
        } else if (strcmp(inFmtStr, "Atmos") == 0) {
            char* pStr = getCmdOption(argv, argv + argc, "-in-file-meta");
            if (pStr && (strlen(pStr) > 0)) {
                m1transcode.setInputFormat(m1transcode.getFormatFromString("CustomPoints"));
                m1audioTimeline.parseAtmos(infilename, pStr);
                useAudioTimeline = true;
            } else {
                cerr << "Please specify an input meta file" << std::endl;
                return -1;
            }
        } else if (strcmp(inFmtStr, "CustomPoints") == 0) {
			pStr = getCmdOption(argv, argv + argc, "-in-json");
			if (pStr && (strlen(pStr) > 0))
            {
                std::ifstream file(pStr);
                std::string strJson((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                m1transcode.setInputFormat(m1transcode.getFormatFromString("CustomPoints"));
				m1transcode.setInputFormatCustomPointsJson((char*)strJson.c_str());
			}
        } else {
            bool foundInFmt = false;
            // rename string for new naming convention
            if (strcmp(inFmtStr, "M1Horizon") == 0) {
                inFmtStr = "M1Spatial-4";
            }
            if (strcmp(inFmtStr, "M1Spatial") == 0) {
                inFmtStr = "M1Spatial-8";
            }
            inFmt = m1transcode.getFormatFromString(inFmtStr);
            if (inFmt > 1) { // if format int is 0 or -1 (making it invalid)
                foundInFmt = true;
            } else {
                std::cout << "Please select a valid input format" << std::endl;
                return -1;
            }
        }
	} else {
		std::cout << "Please select a valid input format" << std::endl;
		return -1;
	}
    
    // input folder
    if (useAudioTimeline) {
        pStr = getCmdOption(argv, argv + argc, "-in-folder");
        if (pStr && (strlen(pStr) > 0)) {
            infolder = pStr;
        } else {
            cerr << "Please specify an input folder for audio files" << std::endl;
            return -1;
        }
    }

	// output file name and format
	pStr = getCmdOption(argv, argv + argc, "-out-file");
	if (pStr && (strlen(pStr) > 0))
	{
		fileOut = true;
		outfilename = pStr;
		std::string FileExt = ".txt";
		int outfilename_size = strlen(outfilename);
		md_outfilename = convertToString(outfilename, outfilename_size);
		std::string Path, FileName;
		std::string::size_type found = md_outfilename.find_last_of(".");
		// if we found one of this symbols
		if (found != std::string::npos) {
			// path will be all symbols before found position
			Path = md_outfilename.substr(0, found);
		}
		else { // if we not found '.', path is empty
			Path.clear();
		}
		md_outfilename += FileExt;
	}
	pStr = getCmdOption(argv, argv + argc, "-out-fmt");
	if (pStr && (strlen(pStr) > 0))
	{
		outFmtStr = pStr;
		if (strcmp(outFmtStr, "CustomPoints") == 0) {
			pStr = getCmdOption(argv, argv + argc, "-out-json");
			if (pStr && (strlen(pStr) > 0))
			{
                std::ifstream file(pStr);
                std::string strJson((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
				m1transcode.setOutputFormatCustomPointsJson((char*)strJson.c_str());
			}
		}
	}

	bool foundOutFmt = false;
	outFmt = m1transcode.getFormatFromString(outFmtStr);
    if (outFmt > 1) { // if format int is 0 or -1 (making it invalid)
		foundOutFmt = true;
	}
	else {
		std::cout << "Please select a valid output format" << std::endl;
		return -1;
	}

	pStr = getCmdOption(argv, argv + argc, "-out-file-chans");
	if (pStr != NULL)
		outFileChans = atoi(pStr);
	else
		outFileChans = 0;
	if (!((outFileChans == 0) || (outFileChans == 1) || (outFileChans == 2)))
	{
		std::cout << "Please select 0, 1, or 2, zero meaning a single, multichannel output file" << std::endl;
		return -1;
	}
	// if "-extract-metadata arg detected, analyze and extract xml metadata
	if (extractMetadata)
	{
		if (admParse.locateMetadata(infilename).mdStartIndex > 0)
		{
			admParse.exportMetadata(infilename, md_outfilename, admParse.locateMetadata(infilename).mdStartIndex, admParse.locateMetadata(infilename).mdEndIndex, admParse.locateMetadata(infilename).totalFileSize);
		}
	}
	std::cout << std::endl;

	//=================================================================
	// initialize inputs, outputs and components
	//

	// -- input file ---------------------------------------
	// determine number of input files
	SndfileHandle *infile[Mach1TranscodeMAXCHANS];
	vector<string> fNames;
    audiofileInfo inputInfo;
    
    if (useAudioTimeline) {
        for (int i = 0; i < audioObjects.size(); i++) {
            std::string filename = std::string(infolder) + "/" + audioObjects[i].getName() + ".wav";
            fNames.push_back(filename);
        }
    } else {
		// check and process the input file, which may be several files separated by a space
		char** itr = std::find(argv, argv + argc, infilename);
		do {
			fNames.push_back(*itr);
		} while (++itr != argv + argc && std::string(*itr).substr(0,1) != "-");
	}
    
	size_t numInFiles = fNames.size();
	for (int i = 0; i < numInFiles; i++) {
		infile[i] = new SndfileHandle(fNames[i].c_str());
		if (infile[i] && (infile[i]->error() == 0)) {
			// print input file stats
			std::cout << "Input File:         " << fNames[i] << std::endl;
            inputInfo = printFileInfo(*infile[i], true);
			sampleRate = (long)infile[i]->samplerate();
			//            int inChannels = 0;
			//            for (int i = 0; i < numInFiles; i++)
			//                inChannels += infile[i]->channels();
			//            parseFile(*infile[i], inChannels);
		} else {
			cerr << "Error: opening in-file: " << fNames[i] << std::endl;
			return -1;
		}
	}

	std::cout << "Master Gain:        " << m1transcode.level2db(masterGain) << "dB" << std::endl;
    std::cout << std::endl;

	for (int i = 0; i < numInFiles; i++) {
		infile[i]->seek(0, 0); // rewind input
	}

	// -- setup 
	m1transcode.setInputFormat(inFmt);
	m1transcode.setOutputFormat(outFmt);
	m1transcode.setLFESub(subChannelIndices, sampleRate);

	// first init of custom points
	if (useAudioTimeline) {
		audioObjects = m1audioTimeline.getAudioObjects();
		std::vector<Mach1Point3D> points;
		for (int i = 0; i < audioObjects.size(); i++) {
			Mach1KeyPoint keypoint = audioObjects[i].getKeyPoints()[0];
			points.push_back(keypoint.point);
		}
		m1transcode.setInputFormatCustomPoints(points);
	}

	// -- output file(s) --------------------------------------

	channels = m1transcode.getOutputNumChannels();
	SndFileWriter outfiles[Mach1TranscodeMAXCHANS];
	int actualOutFileChannels = outFileChans == 0 ? channels : outFileChans;

	if (actualOutFileChannels == 0) {
		std::cout << "Output channels count is 0!" << std::endl;
		return -1;
	}

	int numOutFiles = channels / actualOutFileChannels;

	for (int i = 0; i < Mach1TranscodeMAXCHANS; i++) {
		memset(inBuffers[i], 0, sizeof(inBuffers[i]));
		memset(outBuffers[i], 0, sizeof(outBuffers[i]));
	}

	//=================================================================
	//  print intermediate formats path
	//
	if (!m1transcode.processConversionPath()) {
		printf("Can't find conversion between formats!");
		return -1;
	} else {
        std::vector<int> formatsConvertionPath = m1transcode.getFormatConversionPath();
		printf("Conversion Path:    ");
		for (int k = 0; k < formatsConvertionPath.size(); k++) {
            printf("%s", m1transcode.getFormatName(formatsConvertionPath[k]).c_str());
			if (k < formatsConvertionPath.size() - 1) {
				printf(" > ");
			}
		}
		printf("\r\n");
	}

	vector<vector<float>> matrix = m1transcode.getMatrixConversion();

	//=================================================================
	//  main sound loop
	// 

	int inChannels = 0;
	for (int i = 0; i < numInFiles; i++)
		inChannels += infile[i]->channels();
	sf_count_t numBlocks = infile[0]->frames() / BUFFERLEN; // files must be the same length
	totalSamples = 0;
	float peak = 0.0f;

    for (int pass = 1, countPasses = ((normalize || spatialDownmixerMode) ? 2 : 1); pass <= countPasses; pass++)
    {
        if (pass == 2) {
            // Mach1 Spatial Downmixer
            // Triggered due to correlation of top vs bottom
            // being higher than threshold
            if (spatialDownmixerMode && (outFmt == m1transcode.getFormatFromString("M1Spatial-8"))) {
                m1transcode.setSpatialDownmixer(corrThreshold);
				if (m1transcode.getSpatialDownmixerPossibility()) {
					/*
					std::vector<float> avgSamplesDiff = spatialDownmixChecker.getAvgSamplesDiff();
					for (int i = 0; i < avgSamplesDiff.size(); i++) {
						printf("Average samples diff: %f\r\n", avgSamplesDiff[i]);
					}
					*/ 

                    // reinitialize inputs and outputs
					outFmt = m1transcode.getFormatFromString("M1Spatial-4");
					m1transcode.setOutputFormat(outFmt);
					m1transcode.processConversionPath();

					channels = m1transcode.getOutputNumChannels();
					actualOutFileChannels = outFileChans == 0 ? channels : outFileChans;
					numOutFiles = channels / actualOutFileChannels;

                    printf("Spatial Downmix:    ");
                    printf("%s", m1transcode.getFormatName(outFmt).c_str());
                    printf("\r\n");
				}
			}

			// normalize
			if (normalize)
			{
				std::cout << "Reducing gain by    " << m1transcode.level2db(peak) << "dB" << std::endl;
                std::cout << std::endl;
				masterGain /= peak;
			}

			totalSamples = 0;
			for (int file = 0; file < numInFiles; file++)
				infile[file]->seek(0, SEEK_SET);
		}

		if (pass == countPasses) {
			// init outfiles
			for (int i = 0; i < numOutFiles; i++) {
				//TODO: expand this out to other output types and better handling from printFileInfo()
				int format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
				int inputFormat = infile[0]->format() & 0xffff;
				if (inputFormat == SF_FORMAT_PCM_16) format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
				if (inputFormat == SF_FORMAT_PCM_24) format = SF_FORMAT_WAV | SF_FORMAT_PCM_24;
				if (inputFormat == SF_FORMAT_PCM_32) format = SF_FORMAT_WAV | SF_FORMAT_PCM_32;
				char outfilestr[1024];
				if (numOutFiles > 1) {
					sprintf(outfilestr, "%s_%0d.wav", outfilename, i);
				}
				else {
					strcpy(outfilestr, outfilename);
				}

                /*
                 Section for writing ADM based metadata to output
                 */
				if (writeMetadata) {
                    // Setup empty metadata chunks
                    bw64::ChnaChunk chnaChunkAdm;
                    std::string axmlChunkAdmCorrectedString;
                    int bitDepth;
                    if (inputInfo.format == SF_FORMAT_PCM_16){
                        bitDepth = 16;
                    } else if (inputInfo.format == SF_FORMAT_PCM_24) {
                        bitDepth = 24;
                    } else if (inputInfo.format == SF_FORMAT_PCM_32) {
                        bitDepth = 32;
                    }
                    
                    // TODO: remove the hardcoded `adm_metadata.h` file and write inline instructions for creating the metadata to scale for all formats
                    if (outFmt == m1transcode.getFormatFromString("M1Spatial-8")){
                        // setup `chna` metadata chunk
                        /// Creates a description of an 8 objects
                        std::vector<ChannelDescType> channelDescType = { {3}, {3}, {3}, {3}, {3}, {3}, {3}, {3} };
                        chnaChunkAdm = fillChnaChunkADMDesc(channelDescType);
                        if (chnaChunkAdm.audioIds().size() != actualOutFileChannels){
                            std::cout << "ERROR: Issue writing `chna` metadata chunk due to mismatching channel count" << std::endl;
                            break;
                        }
                        // setup `axml` metadata chunk
                        axmlChunkAdmCorrectedString = prepareAdmMetadata(axml_m1spatial_ChunkAdmString, inputInfo.duration, inputInfo.sampleRate, bitDepth).c_str();
                        bw64::AxmlChunk axmlChunkAdmCorrected(axmlChunkAdmCorrectedString);
                        outfiles[i].open(outfilestr, inputInfo.sampleRate, actualOutFileChannels, bitDepth, chnaChunkAdm, axmlChunkAdmCorrected);
                    }
                    else if (outFmt == m1transcode.getFormatFromString("7.1.2_M") || outFmt == m1transcode.getFormatFromString("7.1.2_C") || outFmt == m1transcode.getFormatFromString("7.1.2_S") || outFmt == m1transcode.getFormatFromString("7.1.2_C_SIM")){
                        // setup `chna` metadata chunk
                        /// Creates a description of an 7.1.2 channel bed
                        std::vector<ChannelDescType> channelDescType = { {1}, {1}, {1}, {1}, {1}, {1}, {1}, {1}, {1}, {1} };
                        chnaChunkAdm = fillChnaChunkADMDesc(channelDescType);
                        if (chnaChunkAdm.audioIds().size() != actualOutFileChannels){
                            std::cout << "ERROR: Issue writing `chna` metadata chunk due to mismatching channel count" << std::endl;
							break;
                        }
                        // setup `axml` metadata chunk
                        axmlChunkAdmCorrectedString = prepareAdmMetadata(axml_7_1_2_ChunkAdmString, inputInfo.duration, inputInfo.sampleRate, bitDepth).c_str();
                        bw64::AxmlChunk axmlChunkAdmCorrected(axmlChunkAdmCorrectedString);
                        outfiles[i].open(outfilestr, inputInfo.sampleRate, actualOutFileChannels, bitDepth, chnaChunkAdm, axmlChunkAdmCorrected);
                    }
                    else if (outFmt == m1transcode.getFormatFromString("5.1.4_M") || outFmt == m1transcode.getFormatFromString("5.1.4_C") || outFmt == m1transcode.getFormatFromString("5.1.4_S")){
                        // setup `chna` metadata chunk
                        /// Creates a description of an 5.1 channel bed + 4 object bed
                        std::vector<ChannelDescType> channelDescType = { {1}, {1}, {1}, {1}, {1}, {1}, {3}, {3}, {3}, {3} };
                        chnaChunkAdm = fillChnaChunkADMDesc(channelDescType);
                        if (chnaChunkAdm.audioIds().size() != actualOutFileChannels){
                            std::cout << "ERROR: Issue writing `chna` metadata chunk due to mismatching channel count" << std::endl;
                            break;
                        }
                        // setup `axml` metadata chunk
                        axmlChunkAdmCorrectedString = prepareAdmMetadata(axml_5_1_4_ChunkAdmString, inputInfo.duration, inputInfo.sampleRate, bitDepth).c_str();
                        bw64::AxmlChunk axmlChunkAdmCorrected(axmlChunkAdmCorrectedString);
                        outfiles[i].open(outfilestr, inputInfo.sampleRate, actualOutFileChannels, bitDepth, chnaChunkAdm, axmlChunkAdmCorrected);
                    }
                    else if (outFmt == m1transcode.getFormatFromString("7.1.4_M") || outFmt == m1transcode.getFormatFromString("7.1.4_C") || outFmt == m1transcode.getFormatFromString("7.1.4_S") || outFmt == m1transcode.getFormatFromString("7.1.4_C_SIM")){
                        // setup `chna` metadata chunk
                        /// Creates a description of an 7.1 channel bed + 4 object bed
                        std::vector<ChannelDescType> channelDescType = { {1}, {1}, {1}, {1}, {1}, {1}, {1}, {1}, {3}, {3}, {3}, {3} };
                        chnaChunkAdm = fillChnaChunkADMDesc(channelDescType);
                        if (chnaChunkAdm.audioIds().size() != actualOutFileChannels){
                            std::cout << "ERROR: Issue writing `chna` metadata chunk due to mismatching channel count" << std::endl;
                            break;
                        }
                        // setup `axml` metadata chunk
                        axmlChunkAdmCorrectedString = prepareAdmMetadata(axml_7_1_4_ChunkAdmString, inputInfo.duration, inputInfo.sampleRate, bitDepth).c_str();
                        bw64::AxmlChunk axmlChunkAdmCorrected(axmlChunkAdmCorrectedString);
                        outfiles[i].open(outfilestr, inputInfo.sampleRate, actualOutFileChannels, bitDepth, chnaChunkAdm, axmlChunkAdmCorrected);
                    }
				}
				else {
					outfiles[i].open(outfilestr, (int)sampleRate, actualOutFileChannels, format);
				}

				if (outfiles[i].isOpened()) {
					// set clipping mode
					outfiles[i].setClip();
					// output file stats
					std::cout << "Output File:        " << outfilestr << std::endl;
					outfiles[i].printInfo();
				}
				else {
					cerr << "Error: opening out-file: " << outfilestr << std::endl;
					return -1;
				}
				if (outFmt == m1transcode.getFormatFromString("M1Spatial") || outFmt == m1transcode.getFormatFromString("M1Spatial-8")) {
					outfiles[i].setString(0x05, "mach1spatial-8");
				}
                else if (outFmt == m1transcode.getFormatFromString("M1Spatial-12")) {
                    outfiles[i].setString(0x05, "mach1spatial-12");
                }
                else if (outFmt == m1transcode.getFormatFromString("M1Spatial-14")) {
                    outfiles[i].setString(0x05, "mach1spatial-14");
                }
                else if (outFmt == m1transcode.getFormatFromString("M1Spatial-32")) {
                    outfiles[i].setString(0x05, "mach1spatial-32");
                }
                else if (outFmt == m1transcode.getFormatFromString("M1Spatial-60")) {
                    outfiles[i].setString(0x05, "mach1spatial-60");
                }
				else if (outFmt == m1transcode.getFormatFromString("M1Horizon") || outFmt == m1transcode.getFormatFromString("M1Spatial-4")) {
					outfiles[i].setString(0x05, "mach1horizon-4");
				}
				else if (outFmt == m1transcode.getFormatFromString("M1HorizonPairs")) {
					outfiles[i].setString(0x05, "mach1horizon-8");
				}
			}
			std::cout << std::endl;
		}

		// get start samples for all objects (ADM format)
		std::vector<long long> startSampleForAudioObject;
		for (int i = 0; i < audioObjects.size(); i++) {
			startSampleForAudioObject.push_back(audioObjects[i].getKeyPoints()[0].sample);
		}

		for (int i = 0; i <= numBlocks; i++) {
			// read next buffer from each infile
			sf_count_t samplesRead = 0;
			sf_count_t firstBuf = 0;
			float(*inBuf)[Mach1TranscodeMAXCHANS][BUFFERLEN] = (float(*)[Mach1TranscodeMAXCHANS][BUFFERLEN])&(inBuffers[0][0]);
			for (int file = 0; file < numInFiles; file++) {
				sf_count_t numChannels = infile[file]->channels();

				// first fill buffer with zeros
				for (int j = 0; j < BUFFERLEN; j++)
					for (int k = 0; k < numChannels; k++) {
						(*inBuf)[firstBuf + k][j] = 0;
					}

				int startSample = 0;
				if (useAudioTimeline) {
					startSample = startSampleForAudioObject[file];
				}
                
				if (totalSamples + BUFFERLEN >= startSample) {
					sf_count_t framesToRead = numChannels * BUFFERLEN;

					// cut samples if the beginning of the offset does not match with the beginning of the buffer
					sf_count_t offset = 0;
					if (startSample + BUFFERLEN < totalSamples && totalSamples < startSample) {
						offset = startSample - totalSamples;
						framesToRead = BUFFERLEN + totalSamples - startSample;
					}

					sf_count_t framesRead = infile[file]->read(fileBuffer, framesToRead);
					samplesRead = framesRead / numChannels;
					// demultiplex into process buffers
					float *ptrFileBuffer = fileBuffer;
					for (int j = 0; j < samplesRead; j++) {
						for (int k = 0; k < numChannels; k++) {
							(*inBuf)[firstBuf + k][offset + j] = *ptrFileBuffer++;
						}
					}
				}

				firstBuf += numChannels;
			}
			totalSamples += samplesRead;

			m1transcode.processConversion(inPtrs, outPtrs, (int)samplesRead);

			if (pass == 1) {
				if (normalize) {
					// find max
					peak = (std::max)(peak, m1transcode.processNormalization(outPtrs, samplesRead));
				}
			}

			if (pass == countPasses) {
				m1transcode.processMasterGain(outPtrs, samplesRead, masterGain);

				// multiplex to output channels with master gain
				float *ptrFileBuffer = fileBuffer;
				float(*outBuf)[Mach1TranscodeMAXCHANS][BUFFERLEN] = (float(*)[Mach1TranscodeMAXCHANS][BUFFERLEN])&(outBuffers[0][0]);

				for (int file = 0; file < numOutFiles; file++) {
                    for (int j = 0; j < samplesRead; j++) {
                        for (int k = 0; k < actualOutFileChannels; k++) {
							*ptrFileBuffer++ = (*outBuf)[(file*actualOutFileChannels) + k][j];
                        }
                    }
                }

				// write to outfile
				for (int j = 0; j < numOutFiles; j++) {
					outfiles[j].write(fileBuffer + (j*actualOutFileChannels*samplesRead), samplesRead);
				}
			}
		}
	}
	// print time played
	std::cout << "Length (sec):       " << (float)totalSamples / (float)sampleRate << std::endl;
	return 0;
}
