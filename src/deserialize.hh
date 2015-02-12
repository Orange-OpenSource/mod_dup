#pragma once

namespace CompareModule {
 
    /**
     * @brief convert a substring of pString in size_t
     * @param pString the string from which to extract a substring to convert
     * @param pLength the length calculated
     * @param msg a message to add to the error
     * @return true if the conversion gets success, false otherwise
     */
    size_t getLength(const std::string pString, const size_t pFirst, const char * msg);
    
    /**
     * @brief extract the request body, the header answer and the response answer
     * @param pReqInfo info of the original request
     * @return a http status
     */
    apr_status_t deserializeBody(DupModule::RequestInfo &pReqInfo);
    
    /**
     * @brief extract the request body, the header answer and the response answer
     * @param pReqInfo info of the original request
     * @return a http status
     */
    apr_status_t deserializeHeader(DupModule::RequestInfo &pReqInfo,const std::string& header);
    
    
    
}