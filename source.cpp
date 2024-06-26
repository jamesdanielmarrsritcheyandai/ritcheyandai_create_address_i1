/*
To compile and install this program on Debian, you need to have g++, and the OpenSSL library installed. If they are not installed, you can install them using the following commands:

First, update your package lists:

sudo apt-get update

Then, install the necessary dependencies:

sudo apt-get install build-essential g++ libssl-dev

Once g++ and OpenSSL are installed, you can compile the program using the following command:

g++ -o program source.cpp -lssl -lcrypto

This command compiles the source.cpp file into an executable named "program" and links the OpenSSL library to the program.

To run the program, use the following command:

./program

If you want to install the program system-wide, you can move the executable to /usr/local/bin using the following command:

sudo mv program /usr/local/bin

After moving the executable, you can run the program from any location using the following command:

program
*/

#include <algorithm>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <getopt.h>
#include <openssl/ec.h>
#include <openssl/obj_mac.h>
#include <openssl/bn.h>
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <sstream>
#include <iomanip> // Include iomanip for setw
#include <openssl/ripemd.h>

void compute_ripemd160(const unsigned char* data, size_t data_len, unsigned char* md) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    const EVP_MD* md_type = EVP_ripemd160();

    if (ctx == nullptr || md_type == nullptr) {
        // Handle error
        return;
    }

    if (EVP_DigestInit_ex(ctx, md_type, nullptr) != 1) {
        // Handle error
        return;
    }

    if (EVP_DigestUpdate(ctx, data, data_len) != 1) {
        // Handle error
        return;
    }

    unsigned int md_len;
    if (EVP_DigestFinal_ex(ctx, md, &md_len) != 1) {
        // Handle error
        return;
    }

    EVP_MD_CTX_free(ctx);
}

// Rest of your program code...

std::string generatePrivateKey(const std::string& input)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    EVP_MD_CTX *mdctx;
    const EVP_MD *md;

    md = EVP_get_digestbyname("sha256");

    if(md == NULL) {
        std::cerr << "Unknown message digest" << std::endl;
        exit(1);
    }

    mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdctx, md, NULL);
    EVP_DigestUpdate(mdctx, input.c_str(), input.size());
    EVP_DigestFinal_ex(mdctx, hash, NULL);
    EVP_MD_CTX_free(mdctx);

    std::stringstream ss;
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return ss.str();
}

std::string generatePublicKey(const std::string& private_key)
{
    EC_GROUP *pGroup = EC_GROUP_new_by_curve_name(NID_secp256k1);
    BIGNUM *pPrivateKeyBN = BN_new();
    BN_hex2bn(&pPrivateKeyBN, private_key.c_str());
    EC_POINT *pPublicKey = EC_POINT_new(pGroup);
    EC_POINT_mul(pGroup, pPublicKey, pPrivateKeyBN, NULL, NULL, NULL);
    char *pPublicKeyHex = EC_POINT_point2hex(pGroup, pPublicKey, POINT_CONVERSION_UNCOMPRESSED, NULL);
    std::string public_key(pPublicKeyHex);
    EC_GROUP_free(pGroup);
    BN_free(pPrivateKeyBN);
    EC_POINT_free(pPublicKey);
    OPENSSL_free(pPublicKeyHex);
    return public_key;
}

std::string base58_chars = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

std::string to_base58(std::string input) {
    std::vector<unsigned char> v(input.begin(), input.end());
    std::string result;
    unsigned int carry;
    while (!v.empty() && v.back() == 0) {
        v.pop_back();
    }
    int size = v.size() * 138 / 100 + 1;
    std::vector<unsigned char> b58(size);
    for (auto& big : v) {
        carry = big;
        for (auto it = b58.rbegin(); it != b58.rend(); it++) {
            carry += 256 * (*it);
            *it = carry % 58;
            carry /= 58;
        }
    }
    auto it = std::find_if(b58.begin(), b58.end(), [](unsigned char x) { return x != 0; });
    for (; it != b58.end(); it++) {
        result += base58_chars[*it];
    }
    return result;
}

std::string generateAddress(const std::string& public_key)
{
    unsigned char hash1[SHA256_DIGEST_LENGTH];
    unsigned char hash2[RIPEMD160_DIGEST_LENGTH];
    unsigned char hash3[SHA256_DIGEST_LENGTH];
    unsigned char hash4[SHA256_DIGEST_LENGTH];
    unsigned char address[25];
    EVP_MD_CTX *mdctx;
    const EVP_MD *md;

    // Step 1: SHA-256 hash
    md = EVP_get_digestbyname("sha256");
    if(md == NULL) {
        std::cerr << "Unknown message digest" << std::endl;
        exit(1);
    }
    mdctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(mdctx, md, NULL);
    EVP_DigestUpdate(mdctx, public_key.c_str(), public_key.size());
    EVP_DigestFinal_ex(mdctx, hash1, NULL);
    EVP_MD_CTX_free(mdctx);

    // Step 2: RIPEMD-160 hash
    compute_ripemd160(hash1, SHA256_DIGEST_LENGTH, hash2);

    // Step 3: Add version byte
    address[0] = 0x00;
    memcpy(address + 1, hash2, RIPEMD160_DIGEST_LENGTH);

    // Step 4 and 5: Double SHA-256 hash
    SHA256(address, RIPEMD160_DIGEST_LENGTH + 1, hash3);
    SHA256(hash3, SHA256_DIGEST_LENGTH, hash4);

    // Step 6: Add checksum
    memcpy(address + 21, hash4, 4);

    // Step 7 and 8: Convert to base58
    std::string address_str(reinterpret_cast<char*>(address), sizeof(address));
    std::string btc_address = to_base58(address_str);

    return btc_address;
}

int main(int argc, char *argv[])
{
    std::string input;
    std::string delimiter = ",";
    std::string stdin_flag = "false";
    std::string source_file;
    std::string destination_file; // new variable for destination file
    int c;

    while (1)
    {
        static struct option long_options[] =
        {
            {"string", required_argument, 0, 's'},
            {"delimiter", required_argument, 0, 'd'},
            {"stdin", required_argument, 0, 'i'},
            {"source_file", required_argument, 0, 'f'},
            {"destination_file", required_argument, 0, 'o'}, // new option
            {0, 0, 0, 0}
        };
        int option_index = 0;
        c = getopt_long(argc, argv, "s:d:i:f:o:", long_options, &option_index); // add 'o' to the options string

        if (c == -1)
            break;

        switch (c)
        {
            case 's':
                input = optarg;
                break;

            case 'd':
                delimiter = optarg;
                break;

            case 'i':
                stdin_flag = optarg;
                break;

            case 'f':
                source_file = optarg;
                break;

            case 'o':
                destination_file = optarg;
                break;

            case '?':
                break;

            default:
                abort();
        }
    }

    if (!source_file.empty())
    {
        std::ifstream file(source_file);
        if (file.is_open())
        {
            std::getline(file, input, '\0');
            file.close();
        }
        else
        {
            std::cerr << "Unable to open file: " << source_file << "\n";
            return 1;
        }
    }
    else if (stdin_flag == "true")
    {
        std::getline(std::cin, input);
    }
    else if (input.empty())
    {
        std::cerr << "You must specify a string using the --string option, set --stdin to true, or provide a source file with --source_file.\n";
        return 1;
    }

    if (delimiter == "\\n")
    {
        delimiter = "\n";
    }
    else if (delimiter == "\\t")
    {
        delimiter = "\t";
    }

    std::string private_key = generatePrivateKey(input);
    std::string public_key = generatePublicKey(private_key);
    std::string address = generateAddress(public_key);

    std::string output = private_key + delimiter + public_key + delimiter + address;

    std::cout << output;

    // If destination file is provided, write to it
    if (!destination_file.empty())
    {
        std::ofstream file(destination_file);
        if (file.is_open())
        {
            file << output;
            file.close();
        }
        else
        {
            std::cerr << "Unable to open file: " << destination_file << "\n";
            return 1;
        }
    }

    return 0;
}