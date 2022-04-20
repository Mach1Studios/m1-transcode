//  Mach1 Spatial SDK
//  Copyright © 2017-2020 Mach1. All rights reserved.

#ifndef ADMParse_h
#define ADMParse_h

class ADMParse
{
public:
    
    struct metadataLocators{
        std::string::size_type mdStartIndex, mdEndIndex, totalFileSize;
    };
    
    /*
     locateMetadata(char* inFile)
     Expects a complete path to a binary file and parses it for XML data
     Upon finding any matches it returns the starting/ending index locations
     of the metadata as well as the total file size for convenience
     
     TODO: make the end index a smarter search (xml end tags)
     */
    metadataLocators locateMetadata(char* inFile)
    {
        metadataLocators locators;
        const char* filename = inFile;
        const char* start_search_term = "<?xml version=";
        size_t search_term_size = strlen(start_search_term);
        std::string::size_type offset, found_at, md_remainder;
        
        std::ifstream file(filename, std::ios::binary);
        if (file)
        {
            file.seekg(0, std::ios::end);
            size_t file_size = file.tellg();
            file.seekg(0, std::ios::beg);
            locators.totalFileSize = file_size;
            std::string file_content;
            
            file_content.reserve(file_size);
            char buffer[16384];
            std::streamsize chars_read;
            
            while (file.read(buffer, sizeof buffer), chars_read = file.gcount())
                file_content.append(buffer, chars_read);
            
            if (file.eof())
            {
                for (offset = 0; file_size > offset && (found_at = file_content.find(start_search_term, offset)) != std::string::npos; offset = found_at + search_term_size)
                {
                    offset = found_at + search_term_size;
                    md_remainder = file_size-offset;
                    locators.mdStartIndex = file_size-md_remainder-search_term_size;
                    
                    //TODO: convert to string, search last use of "</", regex the rest of term to ">",
                    locators.mdEndIndex = file_size;
                    //std::cout << "\n" << "Found Match at Char Index: " << found_at << "\n" << "Line: " << (found_at/24)+1 << " | Char: " << found_at%24 << "\n";
                }
            }
        }
        file.close();
        return locators;
    }
    
    /*
     exportMetadata(inputPath, outputPath, metadataStartingIndex, metadataEndingIndex, totalAudioFileSize)
     Parses metadata and exports it to a new file with .txt appended
     
     TODO: make endIndex = filesize for safety
     */
    void exportMetadata(char* inFile, char* outPath, std::string::size_type mdStartIndex, std::string::size_type mdEndIndex, std::string::size_type fileSize)
    {
        const char* filename = inFile;
        std::ifstream file(filename, std::ios::binary);
        std::ofstream mdOutFile(outPath, std::ios::binary);
        if (file)
        {
            file.seekg(mdStartIndex, std::ios::beg); // seek to end of file where matched xml tag starts
            size_t file_size = fileSize-mdStartIndex;
            //TODO: regex out and rename file
            char* F = new char[file_size];
            file.read(F, file_size);
            F[file_size] = 0; //write end byte
            mdOutFile << F << std::endl;
            printXMLInfo(F);
			delete[] F;
            mdOutFile.close();
        }
    }
    
    /*
     exportMetadata(inputPath, outputPath, metadataStartingIndex, metadataEndingIndex, totalAudioFileSize)
     Parses metadata and exports it to a new file with .txt appended
     
     TODO: make endIndex = filesize for safety
     */
    void exportMetadata(std::string inFile, std::string outPath, std::string::size_type mdStartIndex, std::string::size_type mdEndIndex, std::string::size_type fileSize)
    {
        std::string filename = inFile;
        std::ifstream file(filename, std::ios::binary);
        std::ofstream mdOutFile(outPath, std::ios::binary);
        if (file)
        {
            file.seekg(mdStartIndex, std::ios::beg); // seek to end of file where matched xml tag starts
            size_t file_size = fileSize-mdStartIndex;
            //TODO: regex out and rename file
			char* F = new char[file_size];
			file.read(F, file_size);
            F[file_size] = 0; //write end byte
            mdOutFile << F << std::endl;
            printXMLInfo(F);
			delete[] F;
            mdOutFile.close();
        }
    }
    
    void printXMLInfo(char* data)
    {
        //TODO: parse datascheme
        //TODO: parse title
        //TODO: parse creator/organization
        //TODO: parse description
        //TODO: parse date
        //TODO: parse format(s)
        //TODO: parse objects
        std::cout << std::endl;
        std::cout << "Raw Metadata:" << std::endl;
        std::cout << data << std::endl;
        std::cout << std::endl;
    }
};

#endif /* ADMParse_h */
