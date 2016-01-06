//#############################################################################
//##  File Name:        STD_SHA_256.cpp                                      ##
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

#include "../include/STD_SHA_256.h"

const uint32_t STD_SHA_256::K [64] =
{
    0x428A2F98,        0x71374491,        0xB5C0FBCF,        0xE9B5DBA5,
    0x3956C25B,        0x59F111F1,        0x923F82A4,        0xAB1C5ED5,
    0xD807AA98,        0x12835B01,        0x243185BE,        0x550C7DC3,
    0x72BE5D74,        0x80DEB1FE,        0x9BDC06A7,        0xC19BF174,
    0xE49B69C1,        0xEFBE4786,        0x0FC19DC6,        0x240CA1CC,
    0x2DE92C6F,        0x4A7484AA,        0x5CB0A9DC,        0x76F988DA,
    0x983E5152,        0xA831C66D,        0xB00327C8,        0xBF597FC7,
    0xC6E00BF3,        0xD5A79147,        0x06CA6351,        0x14292967,
    0x27B70A85,        0x2E1B2138,        0x4D2C6DFC,        0x53380D13,
    0x650A7354,        0x766A0ABB,        0x81C2C92E,        0x92722C85,
    0xA2BFE8A1,        0xA81A664B,        0xC24B8B70,        0xC76C51A3,
    0xD192E819,        0xD6990624,        0xF40E3585,        0x106AA070,
    0x19A4C116,        0x1E376C08,        0x2748774C,        0x34B0BCB5,
    0x391C0CB3,        0x4ED8AA4A,        0x5B9CCA4F,        0x682E6FF3,
    0x748F82EE,        0x78A5636F,        0x84C87814,        0x8CC70208,
    0x90BEFFFA,        0xA4506CEB,        0xBEF9A3F7,        0xC67178F2
};

//-----------------------------------------------------------------------------
    STD_SHA_256::STD_SHA_256 ()
//-----------------------------------------------------------------------------
{
    Reset ();
}

//-----------------------------------------------------------------------------
    void STD_SHA_256::Reset ()
//-----------------------------------------------------------------------------
{
    my_Total_Bytes        = 0;
    my_Padding_is_Done    = false;

    my_Hash_Value [0] = 0x6A09E667;
    my_Hash_Value [1] = 0xBB67AE85;
    my_Hash_Value [2] = 0x3C6EF372;
    my_Hash_Value [3] = 0xA54FF53A;
    my_Hash_Value [4] = 0x510E527F;
    my_Hash_Value [5] = 0x9B05688C;
    my_Hash_Value [6] = 0x1F83D9AB;
    my_Hash_Value [7] = 0x5BE0CD19;
}

//-----------------------------------------------------------------------------
    void STD_SHA_256::Add (uint8_t Byte)
//-----------------------------------------------------------------------------
{
    if (my_Padding_is_Done)
        // A message has already been added and its digest calculated,
        // hence this is the first byte of a new message
        Reset ();

    my_Block [my_Total_Bytes % 64] = Byte;
    my_Total_Bytes++;
    if (my_Total_Bytes % 64 == 0)
        Process_Block ();
}

//-----------------------------------------------------------------------------
    uint32_t STD_SHA_256::Rotated_Right (uint32_t    Word,
                                                 unsigned int        Times)
//-----------------------------------------------------------------------------
// Circular right shift (rotation) of Word by Times positions to the right.
// Times >= 0 && Times < 32
{
    return (Word >> Times) | (Word << (32 - Times));
}

//-----------------------------------------------------------------------------
    uint32_t STD_SHA_256::Shifted_Right (uint32_t    Word,
                                                 unsigned int        Times)
//-----------------------------------------------------------------------------
{
    return Word >> Times;
}

//-----------------------------------------------------------------------------
    uint32_t STD_SHA_256::Upper_Sigma_0 (uint32_t Word)
//-----------------------------------------------------------------------------
{
    return   Rotated_Right (Word, 2)
           ^ Rotated_Right (Word, 13)
           ^ Rotated_Right (Word, 22);
}

//-----------------------------------------------------------------------------
    uint32_t STD_SHA_256::Upper_Sigma_1 (uint32_t Word)
//-----------------------------------------------------------------------------
{
    return   Rotated_Right (Word, 6)
           ^ Rotated_Right (Word, 11)
           ^ Rotated_Right (Word, 25);
}

//-----------------------------------------------------------------------------
    uint32_t STD_SHA_256::Lower_Sigma_0 (uint32_t Word)
//-----------------------------------------------------------------------------
{
    return   Rotated_Right (Word, 7)
           ^ Rotated_Right (Word, 18)
           ^ Shifted_Right (Word, 3);
}

//-----------------------------------------------------------------------------
    uint32_t STD_SHA_256::Lower_Sigma_1 (uint32_t Word)
//-----------------------------------------------------------------------------
{
    return   Rotated_Right (Word, 17)
           ^ Rotated_Right (Word, 19)
           ^ Shifted_Right (Word, 10);
}

//-----------------------------------------------------------------------------
    uint32_t STD_SHA_256::Ch (uint32_t Word_1,
                                      uint32_t Word_2,
                                      uint32_t Word_3)
//-----------------------------------------------------------------------------
{
    return (Word_1 & Word_2) ^ (~Word_1 & Word_3);
}

//-----------------------------------------------------------------------------
    uint32_t STD_SHA_256::Maj (uint32_t Word_1,
                                       uint32_t Word_2,
                                       uint32_t Word_3)
//-----------------------------------------------------------------------------
{
    return (Word_1 & Word_2) ^ (Word_1 & Word_3) ^ (Word_2 & Word_3);
}

//-----------------------------------------------------------------------------
    void STD_SHA_256::Padd ()
//-----------------------------------------------------------------------------
{
    uint64_t Total_Message_Bits = my_Total_Bytes * 8;

    Add (0x80);
    while (my_Total_Bytes % 64 != 56)
        Add (0);
    Add ((uint8_t) (Total_Message_Bits >> 54));
    Add ((uint8_t) (Total_Message_Bits >> 48));
    Add ((uint8_t) (Total_Message_Bits >> 40));
    Add ((uint8_t) (Total_Message_Bits >> 32));
    Add ((uint8_t) (Total_Message_Bits >> 24));
    Add ((uint8_t) (Total_Message_Bits >> 16));
    Add ((uint8_t) (Total_Message_Bits >> 8));
    Add ((uint8_t) (Total_Message_Bits));

    my_Padding_is_Done = true;
}

//-----------------------------------------------------------------------------
    void STD_SHA_256::Process_Block ()
//-----------------------------------------------------------------------------
{
    unsigned int                i;
    uint32_t            Schedule [64];
    uint32_t            a, b, c, d, e, f, g, h;
    uint32_t            T1, T2;

    for (i = 0; i <= 15; i++)
    {
        Schedule [i] = my_Block [i * 4];
        Schedule [i] <<= 8;
        Schedule [i] += my_Block [i * 4 + 1];
        Schedule [i] <<= 8;
        Schedule [i] += my_Block [i * 4 + 2];
        Schedule [i] <<= 8;
        Schedule [i] += my_Block [i * 4 + 3];
    }

    for (i = 16; i <= 63; i++)
        Schedule [i] =   Lower_Sigma_1 (Schedule [i - 2])  + Schedule [i - 7]
                       + Lower_Sigma_0 (Schedule [i - 15]) + Schedule [i - 16];

    a = my_Hash_Value [0];
    b = my_Hash_Value [1];
    c = my_Hash_Value [2];
    d = my_Hash_Value [3];
    e = my_Hash_Value [4];
    f = my_Hash_Value [5];
    g = my_Hash_Value [6];
    h = my_Hash_Value [7];

    for (i = 0; i <= 63; i++)
    {
        T1 = h + Upper_Sigma_1 (e) + Ch (e, f, g) + K [i] + Schedule [i];
        T2 = Upper_Sigma_0 (a) + Maj (a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + T1;
        d = c;
        c = b;
        b = a;
        a = T1 + T2;
    }

    my_Hash_Value [0] += a;
    my_Hash_Value [1] += b;
    my_Hash_Value [2] += c;
    my_Hash_Value [3] += d;
    my_Hash_Value [4] += e;
    my_Hash_Value [5] += f;
    my_Hash_Value [6] += g;
    my_Hash_Value [7] += h;
}

//-----------------------------------------------------------------------------
    uint8_t STD_SHA_256::Digest (unsigned int Index)
//-----------------------------------------------------------------------------
// Index in 0 .. 31
{
    unsigned int    i = Index / 4;                // 0 to 7
    unsigned int    s = (3 - Index % 4) * 8;    // 0, 8, 16 or 24

    if (! my_Padding_is_Done)
        Padd ();

    return (uint8_t) Shifted_Right (my_Hash_Value [i], s);
}

//-----------------------------------------------------------------------------
    std::string STD_SHA_256::Digest ()
//-----------------------------------------------------------------------------
{
    std::string        Hex_Digest = "";
    unsigned int    Nibble;

    for (unsigned int i = 0; i < 64; i++)
    {
        if (i % 2 == 0)
            Nibble = Digest (i / 2) >> 4;
        else
            Nibble = Digest (i / 2) & 0x0F;

        if (Nibble >= 10)
            Hex_Digest += 'a' + Nibble - 10;
        else
            Hex_Digest += '0' + Nibble;
    }

    return Hex_Digest;
}

//-----------------------------------------------------------------------------
    void STD_SHA_256::Get_Digest (void *            Message,
                                  unsigned int        Message_Length,
                                  uint8_t    Digest [32])
//-----------------------------------------------------------------------------
{
    uint8_t *    Data = (uint8_t *) Message;
    unsigned int        i;
    STD_SHA_256            SHA;

    for (i = 0; i < Message_Length; i++)
        SHA.Add (Data [i]);

    for (i = 0; i < 32; i++)
        Digest [i] = SHA.Digest (i);
}

//-----------------------------------------------------------------------------
    std::string    STD_SHA_256::Digest (std::string Message)
//-----------------------------------------------------------------------------
{
    STD_SHA_256 SHA;

    for (unsigned int i = 0; i < Message.size (); i++)
        SHA.Add (Message [i]);

    return SHA.Digest ();
}

//-----------------------------------------------------------------------------
    STD_SHA_256::~STD_SHA_256 ()
//-----------------------------------------------------------------------------
{

}
