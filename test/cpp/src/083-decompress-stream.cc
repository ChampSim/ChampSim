#include <catch.hpp>

#include "inf_stream.h"

const std::string plaintext{
"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum."
};

const std::string gzip_cyphertext{{
  '\x1f', '\x8b', '\x08', '\x08', '\xdd', '\x3e', '\x22', '\x63', '\x00', '\x03', '\x31', '\x2e', '\x74', '\x78', '\x74', '\x00',
  '\x35', '\x90', '\xc1', '\x71', '\x43', '\x31', '\x08', '\x44', '\xef', '\xa9', '\x62', '\x0b', '\xf0', '\xfc', '\x2a', '\x92',
  '\x5b', '\xae', '\x29', '\x80', '\x20', '\xec', '\x30', '\x23', '\x09', '\x59', '\x02', '\x8f', '\xcb', '\x0f', '\xf2', '\x4f',
  '\x6e', '\x42', '\xc0', '\xb2', '\xfb', '\x3e', '\x6d', '\x4a', '\x83', '\x8e', '\x15', '\x0d', '\xc5', '\xaa', '\x4d', '\x2c',
  '\x75', '\x50', '\x13', '\xbf', '\x80', '\xad', '\x2f', '\x61', '\x17', '\x8f', '\x09', '\x2a', '\x3a', '\x74', '\xb1', '\xf6',
  '\x1b', '\xa4', '\x6a', '\x36', '\x97', '\x94', '\x5c', '\x80', '\x68', '\xac', '\x66', '\x05', '\x2e', '\x6d', '\xe4', '\xb2',
  '\x76', '\xd6', '\xa2', '\x25', '\xba', '\x23', '\x1c', '\x95', '\xbe', '\x53', '\x1e', '\xe2', '\xa7', '\xb4', '\xa0', '\xd1',
  '\xad', '\x13', '\xa8', '\xea', '\x3d', '\xe8', '\xc0', '\x97', '\x43', '\xba', '\xb6', '\xd4', '\x46', '\xd3', '\xfd', '\x78',
  '\x64', '\x49', '\xed', '\x82', '\x7b', '\xe8', '\x42', '\xb7', '\xe5', '\x33', '\x0a', '\xe4', '\x29', '\x93', '\xd5', '\xc9',
  '\xd5', '\x3a', '\xa2', '\x56', '\x6a', '\x6c', '\xa7', '\xf2', '\x1e', '\xd2', '\xa5', '\xfb', '\xd2', '\x4b', '\x52', '\x47',
  '\x0e', '\x43', '\x28', '\x8d', '\xb7', '\xf4', '\x64', '\x67', '\x80', '\x3c', '\xe5', '\x07', '\xde', '\xb7', '\x24', '\x85',
  '\x0b', '\x74', '\x46', '\x3a', '\x39', '\xb3', '\x6a', '\xc7', '\x94', '\x31', '\xe5', '\x47', '\x7a', '\x91', '\x99', '\xc1',
  '\xf3', '\xe3', '\x61', '\x35', '\x46', '\x9e', '\x93', '\xb4', '\x93', '\x49', '\x21', '\x6b', '\x09', '\x58', '\x6b', '\xfd',
  '\x27', '\x94', '\x81', '\x02', '\xd7', '\xb8', '\x29', '\x39', '\xfa', '\x36', '\x84', '\x41', '\x33', '\x8b', '\x98', '\x07',
  '\x3e', '\x9e', '\x2c', '\xc3', '\x25', '\x36', '\xc6', '\x64', '\x60', '\xcc', '\x24', '\x9c', '\x73', '\x1c', '\x43', '\x0b',
  '\xf9', '\xde', '\xc8', '\x14', '\x63', '\x9a', '\x16', '\xe9', '\x9b', '\xe2', '\x26', '\x95', '\x47', '\x39', '\xea', '\xa0',
  '\x9d', '\x1b', '\x76', '\xbd', '\x2a', '\x2b', '\xa1', '\xc8', '\x92', '\xb9', '\xbb', '\xcd', '\xea', '\xb6', '\x41', '\x1b',
  '\x90', '\x26', '\x8e', '\xf5', '\xc7', '\x35', '\xda', '\xf1', '\xf6', '\x0b', '\x67', '\x7b', '\x9f', '\x87', '\xbe', '\x01',
  '\x00', '\x00'
}};

const std::string xz_cyphertext{{
  '\xfd', '\x37', '\x7a', '\x58', '\x5a', '\x00', '\x00', '\x04', '\xe6', '\xd6', '\xb4', '\x46', '\x02', '\x00', '\x21', '\x01',
  '\x16', '\x00', '\x00', '\x00', '\x74', '\x2f', '\xe5', '\xa3', '\xe0', '\x01', '\xbd', '\x01', '\x3e', '\x5d', '\x00', '\x26',
  '\x1b', '\xca', '\x46', '\x67', '\x5a', '\xf2', '\x77', '\xb8', '\x7d', '\x86', '\xd8', '\x41', '\xdb', '\x05', '\x35', '\xcd',
  '\x83', '\xa5', '\x7c', '\x12', '\xa5', '\x05', '\xdb', '\x90', '\xbd', '\x2f', '\x14', '\xd3', '\x71', '\x72', '\x96', '\xa8',
  '\x8a', '\x7d', '\x84', '\x56', '\x71', '\x8d', '\x6a', '\x22', '\x98', '\xab', '\x9e', '\x3d', '\xc3', '\x55', '\xef', '\xcc',
  '\xa5', '\xc3', '\xdd', '\x5b', '\x8e', '\xbf', '\x03', '\x81', '\x21', '\x40', '\xd6', '\x26', '\x91', '\x02', '\x45', '\x4f',
  '\x92', '\xa1', '\x78', '\xbb', '\x8a', '\x00', '\xaf', '\x90', '\x2a', '\x26', '\x92', '\x02', '\x23', '\xe5', '\x5c', '\xb3',
  '\x2d', '\xe3', '\xe8', '\x5c', '\x2c', '\xfb', '\x32', '\x21', '\xc6', '\x6f', '\x6a', '\x37', '\xb1', '\x66', '\x20', '\xcd',
  '\xb7', '\x52', '\x7d', '\x66', '\xa4', '\x21', '\x08', '\xd1', '\x44', '\x14', '\x6c', '\x7d', '\x34', '\x90', '\x6d', '\xd6',
  '\x47', '\xad', '\x5d', '\x5a', '\x90', '\x76', '\x28', '\xc8', '\xe7', '\x8f', '\x78', '\x22', '\x47', '\x07', '\x17', '\x9e',
  '\x9d', '\x95', '\x7f', '\x6f', '\x30', '\xa4', '\xe0', '\x3a', '\x53', '\xb7', '\x14', '\xb6', '\x42', '\x9d', '\x20', '\xc2',
  '\xfd', '\x88', '\xb4', '\x49', '\xb1', '\xb6', '\xf7', '\xdb', '\x8c', '\x7f', '\xe2', '\x9d', '\x58', '\x9f', '\x66', '\x55',
  '\x01', '\x44', '\x9e', '\x4c', '\x21', '\x6c', '\x4d', '\x46', '\x3c', '\x16', '\x9f', '\xf5', '\x53', '\xaa', '\x19', '\xe2',
  '\xd6', '\x4b', '\x56', '\xc2', '\x19', '\xd0', '\xc1', '\x3c', '\x5b', '\x8b', '\x1a', '\x26', '\xe8', '\xb8', '\x41', '\xa5',
  '\xb8', '\x25', '\x75', '\x94', '\x0c', '\xe5', '\x98', '\xc9', '\xe3', '\xdd', '\x82', '\xbf', '\x46', '\x45', '\x72', '\x04',
  '\xa2', '\x88', '\xf1', '\xa9', '\x07', '\xd6', '\xe2', '\xd6', '\xa4', '\x5f', '\xcc', '\xd5', '\x91', '\xa4', '\x64', '\xac',
  '\xb1', '\x11', '\xae', '\xbd', '\x20', '\x5f', '\x89', '\x5a', '\x04', '\x36', '\x98', '\x79', '\x54', '\xdd', '\x29', '\x26',
  '\x5e', '\x70', '\x1d', '\xf4', '\x87', '\xc3', '\x3d', '\xe6', '\x4a', '\x8b', '\x37', '\x87', '\x26', '\x1f', '\x6c', '\x1b',
  '\x10', '\x7c', '\x43', '\x72', '\xff', '\xef', '\x57', '\x3e', '\xa2', '\x38', '\xca', '\xca', '\x51', '\xc1', '\x12', '\x8b',
  '\xd2', '\x69', '\x6f', '\xf7', '\x09', '\xad', '\xe2', '\x7a', '\x3f', '\x40', '\xc7', '\x3a', '\xe9', '\xbd', '\xf2', '\x25',
  '\x5e', '\x39', '\xcf', '\xcf', '\x10', '\x30', '\xb0', '\x4f', '\x7a', '\x72', '\x07', '\x21', '\xed', '\x25', '\xdb', '\x5a',
  '\xec', '\x0d', '\x0a', '\x51', '\x74', '\x6b', '\xbc', '\x25', '\x0a', '\x92', '\x01', '\x6e', '\x00', '\x00', '\x00', '\x00',
  '\xbc', '\xfd', '\xfc', '\x00', '\x58', '\xe0', '\x91', '\xd4', '\x00', '\x01', '\xda', '\x02', '\xbe', '\x03', '\x00', '\x00',
  '\x04', '\x3f', '\xdb', '\xf5', '\xb1', '\xc4', '\x67', '\xfb', '\x02', '\x00', '\x00', '\x00', '\x00', '\x04', '\x59', '\x5a'
}};

const std::string bz2_cyphertext{{
  '\x42', '\x5a', '\x68', '\x39', '\x31', '\x41', '\x59', '\x26', '\x53', '\x59', '\x9f', '\x43', '\x32', '\xad', '\x00', '\x00',
  '\x27', '\xd7', '\x80', '\x00', '\x10', '\x40', '\x05', '\x06', '\x04', '\x02', '\x00', '\x3f', '\xe7', '\xff', '\x40', '\x30',
  '\x01', '\x2d', '\xb6', '\x36', '\x22', '\x7a', '\x4c', '\x81', '\x30', '\x8d', '\x47', '\xa9', '\xea', '\x7a', '\x9e', '\x50',
  '\x6a', '\x69', '\xe8', '\xd5', '\x3c', '\xa4', '\xda', '\x99', '\x00', '\xc8', '\xd0', '\x6a', '\x79', '\x04', '\x4c', '\x4c',
  '\xa3', '\x44', '\x0f', '\x50', '\x69', '\x15', '\xf3', '\x2d', '\x33', '\x5d', '\xb6', '\x39', '\xcb', '\x92', '\xee', '\x89',
  '\x27', '\x1f', '\x86', '\x93', '\x9c', '\x5a', '\x3d', '\xc1', '\x68', '\x01', '\xb9', '\xb0', '\x06', '\xe3', '\xa0', '\x00',
  '\xbf', '\x33', '\xc9', '\xd2', '\x37', '\x06', '\xad', '\x13', '\x6b', '\xbb', '\x22', '\x09', '\xad', '\xbc', '\x8f', '\x26',
  '\x6b', '\x6e', '\xf7', '\xb5', '\x49', '\x1f', '\x79', '\x42', '\x5d', '\x09', '\x8c', '\xc6', '\x58', '\x20', '\xad', '\x2c',
  '\xb3', '\xdb', '\xba', '\xc6', '\x5d', '\xb6', '\xd4', '\xda', '\x58', '\x32', '\xc7', '\x4c', '\xc8', '\xa5', '\x77', '\x73',
  '\x60', '\x6a', '\xad', '\xa3', '\x33', '\xa7', '\x08', '\xde', '\x03', '\x5d', '\xa2', '\x59', '\x6c', '\xfb', '\x21', '\x85',
  '\x65', '\xa2', '\x60', '\x14', '\xf6', '\x75', '\xb5', '\x7b', '\x39', '\x4d', '\x71', '\x58', '\xc6', '\xfd', '\x3e', '\xa2',
  '\x0c', '\x52', '\x25', '\xab', '\xeb', '\x35', '\x9f', '\x40', '\xb6', '\x4e', '\x2f', '\x69', '\x10', '\x6f', '\xa5', '\x6a',
  '\x1d', '\x82', '\xc3', '\x81', '\xcf', '\xa2', '\x02', '\x74', '\xcc', '\x0b', '\x98', '\x69', '\x40', '\xc7', '\x1a', '\x6a',
  '\xd6', '\x09', '\x3e', '\x0b', '\x12', '\x2a', '\xa2', '\x90', '\x2c', '\x18', '\xc3', '\xe8', '\x61', '\x70', '\x0e', '\x53',
  '\x81', '\xd4', '\x6b', '\x84', '\x35', '\xb3', '\xfa', '\x47', '\x68', '\x4c', '\xbe', '\x39', '\x6c', '\x72', '\xec', '\xec',
  '\x8b', '\x22', '\xda', '\x04', '\x97', '\x2a', '\x97', '\x2f', '\xb5', '\x0f', '\xd3', '\x35', '\x68', '\x3a', '\xc4', '\xb3',
  '\xb9', '\x14', '\x42', '\x97', '\x78', '\x14', '\xbf', '\x19', '\x68', '\xa2', '\x83', '\x05', '\x17', '\x22', '\x4a', '\x33',
  '\xac', '\x19', '\x9b', '\xb7', '\x23', '\xc7', '\xab', '\x96', '\xc4', '\xe5', '\x28', '\xf9', '\x03', '\x18', '\x44', '\xf3',
  '\xa0', '\xb6', '\x81', '\x50', '\x31', '\x78', '\x3f', '\x8b', '\xb9', '\x22', '\x9c', '\x28', '\x48', '\x4f', '\xa1', '\x99',
  '\x56', '\x80'
}};

TEST_CASE("An inf_stream can inflate a gzip-compressed text") {
  // Initialize a inflation/deflation buffer
  champsim::inf_istream<champsim::decomp_tags::gzip_tag_t<>, std::istringstream> comp_stream{std::istringstream{gzip_cyphertext}};

  STATIC_REQUIRE(std::is_move_constructible<decltype(comp_stream)>::value);
  STATIC_REQUIRE(std::is_move_assignable<decltype(comp_stream)>::value);
  STATIC_REQUIRE(std::is_swappable<decltype(comp_stream)>::value);

  char inflated[1000] = {};
  comp_stream.read(inflated, static_cast<std::streamsize>(std::size(plaintext)));
  REQUIRE_THAT(std::string{inflated}, Catch::Matchers::Equals(plaintext));
}

TEST_CASE("An inf_stream can inflate a xz-compressed text") {
  // Initialize a inflation/deflation buffer
  champsim::inf_istream<champsim::decomp_tags::lzma_tag_t<>, std::istringstream> comp_stream{std::istringstream{xz_cyphertext}};

  STATIC_REQUIRE(std::is_move_constructible<decltype(comp_stream)>::value);
  STATIC_REQUIRE(std::is_move_assignable<decltype(comp_stream)>::value);
  STATIC_REQUIRE(std::is_swappable<decltype(comp_stream)>::value);

  char inflated[1000] = {};
  comp_stream.read(inflated, static_cast<std::streamsize>(std::size(plaintext)));
  REQUIRE_THAT(std::string{inflated}, Catch::Matchers::Equals(plaintext));
}

TEST_CASE("An inf_stream can inflate a bz2-compressed text") {
  // Initialize a inflation/deflation buffer
  champsim::inf_istream<champsim::decomp_tags::bzip2_tag_t, std::istringstream> comp_stream{std::istringstream{bz2_cyphertext}};

  STATIC_REQUIRE(std::is_move_constructible<decltype(comp_stream)>::value);
  STATIC_REQUIRE(std::is_move_assignable<decltype(comp_stream)>::value);
  STATIC_REQUIRE(std::is_swappable<decltype(comp_stream)>::value);

  char inflated[1000] = {};
  comp_stream.read(inflated, static_cast<std::streamsize>(std::size(plaintext)));
  REQUIRE_THAT(std::string{inflated}, Catch::Matchers::Equals(plaintext));
}
