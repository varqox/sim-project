//#############################################################################
//##  File Name:        STD_SHA_256.h                                        ##
//##  File Version:     1.00 (19 Jul 2008)                                   ##
//##  Author:           Silvestro Fantacci                                   ##
//##  Copyright:        Public Domain                                        ##
//#############################################################################
//
//  THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
//  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//  DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
//  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
//  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
//  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
//  HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
//  STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
//  IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
//  POSSIBILITY OF SUCH DAMAGE.

#if !defined (STD_SHA_256_INCLUDED)
#define STD_SHA_256_INCLUDED

#include <stdint.h>
#include <string>

//=============================================================================
    class STD_SHA_256
//=============================================================================
// This class implements a SHA-256 secure hash algorithm as specified in the
// "Secure Hash Standard (SHS)" document FIPS PUB 180-2, August 2002.
// The SHA-256 algorithm calculates a 32 byte (256 bit) number - called a
// "message digest" - from an input message of any length from 0 to 2^64 - 1
// bits.
// The algorithm implemented in this class is byte oriented, i.e. the input
// message resolution is in bytes and its length can range from 0 to 2^61 - 1
// bytes.
{
public:

    //--------  Two Phase Calculation  --------

    // Phase 1 - Add the whole message, byte by byte
    // Phase 2 - Inspect the calculated digest

    // Note: calls to the Digest methods are taken to imply that the whole
    // message has been added. Hence a subsequent call to Add, if any, will
    // be interpreted as the restart of Phase 1 on a new message.

                        STD_SHA_256 ();

    void                Add (uint8_t Byte);

    // Index in 0 .. 31
    uint8_t        Digest (unsigned int Index);

    // Returns the digest's hexadecimal representation.
    // 64 charactes long, A to F uppercase.
    std::string            Digest ();

    // To explicitly request the restart of Phase 1 on a new message.
    void                Reset ();

    virtual                ~STD_SHA_256 ();

    //--------  Oneshot Calculations  --------

    // Message_Length is in bytes
    static void            Get_Digest (void *                Message,
                                    unsigned int        Message_Length,
                                    uint8_t        Digest [32]);

    // The digest's hexadecimal representation.
    // The string is 64 charactes long, with A to F uppercase.
    // E.g. Digest ("abc") == "BA7816BF8F01CFEA414140DE5DAE2223"
    //                        "B00361A396177A9CB410FF61F20015AD"
    static std::string    Digest (std::string Message);

private:

    static uint32_t    Rotated_Right (uint32_t        Word,
                                           unsigned int            Times);

    static uint32_t    Shifted_Right (uint32_t        Word,
                                           unsigned int            Times);

    static uint32_t    Upper_Sigma_0 (uint32_t    Word);

    static uint32_t    Upper_Sigma_1 (uint32_t    Word);

    static uint32_t    Lower_Sigma_0 (uint32_t    Word);

    static uint32_t    Lower_Sigma_1 (uint32_t    Word);

    static uint32_t    Ch (uint32_t Word_1,
                                uint32_t Word_2,
                                uint32_t Word_3);

    static uint32_t    Maj (uint32_t Word_1,
                                 uint32_t Word_2,
                                 uint32_t Word_3);

    void                    Padd ();

    void                    Process_Block ();

    // A message block of 64 * 8 = 512 bit
    uint8_t            my_Block [64];

    uint32_t        my_Hash_Value [8];
    uint64_t        my_Total_Bytes;
    bool                    my_Padding_is_Done;

    static const uint32_t    K [64];
};

#endif // !defined (STD_SHA_256_INCLUDED)
