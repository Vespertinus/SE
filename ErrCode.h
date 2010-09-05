
#ifndef __ERR_CODE_H__
#define __ERR_CODE_H__ 1

namespace SE {

const ret_code_t  uSUCCESS                  = 0x0;
const ret_code_t  uREAD_FILE_ERROR          = 0x00001001;
const ret_code_t  uWRITE_FILE_ERROR         = 0x00001101;
const ret_code_t  uMEMORY_ALLOCATION_ERROR  = 0x00001201;
const ret_code_t  uWRONG_INPUT_DATA         = 0x00001302;

const ret_code_t  uFATAL_ERROR              = 0xDEADBEEF;
const ret_code_t  uUNKNOWN_ERROR            = 0xBEEC0DED;

} // namespace SE

#endif
