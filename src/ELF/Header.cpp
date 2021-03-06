/* Copyright 2017 R. Thomas
 * Copyright 2017 Quarkslab
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <set>
#include <map>
#include <iomanip>
#include <sstream>
#include <numeric>

#include "LIEF/exception.hpp"
#include "LIEF/visitors/Hash.hpp"

#include "LIEF/ELF/Header.hpp"
#include "LIEF/ELF/EnumToString.hpp"

namespace LIEF {
namespace ELF {

static const std::map<ARCH, std::pair<ARCHITECTURES, std::set<MODES>>> arch_elf_to_lief {
  {ARCH::EM_NONE,      {ARCH_NONE,  {}}},
  {ARCH::EM_X86_64,    {ARCH_X86,   {MODE_64}}},
  {ARCH::EM_ARM,       {ARCH_ARM,   {MODE_32}}},
  {ARCH::EM_AARCH64,   {ARCH_ARM64, {MODE_64}}},
  {ARCH::EM_386,       {ARCH_X86,   {MODE_32}}},
  {ARCH::EM_IA_64,     {ARCH_INTEL, {MODE_64}}},
};

static const std::map<E_TYPE, OBJECT_TYPES> obj_elf_to_lief {
  {E_TYPE::ET_EXEC, OBJECT_TYPES::TYPE_EXECUTABLE},
  {E_TYPE::ET_DYN,  OBJECT_TYPES::TYPE_LIBRARY},
  {E_TYPE::ET_REL,  OBJECT_TYPES::TYPE_OBJECT},
};

static const std::map<ELF_DATA, ENDIANNESS> endi_elf_to_lief {
  {ELF_DATA::ELFDATANONE, ENDIANNESS::ENDIAN_NONE},
  {ELF_DATA::ELFDATA2LSB, ENDIANNESS::ENDIAN_LITTLE},
  {ELF_DATA::ELFDATA2MSB, ENDIANNESS::ENDIAN_BIG},
};


Header& Header::operator=(const Header&) = default;
Header::Header(const Header&)            = default;
Header::~Header(void)                    = default;

Header::Header(void) :
  file_type_{E_TYPE::ET_NONE},
  machine_type_{ARCH::EM_NONE},
  object_file_version_{VERSION::EV_NONE},
  entrypoint_(0),
  program_headers_offset_(0),
  section_headers_offset_(0),
  processor_flags_(0),
  header_size_(0),
  program_header_size_(0),
  numberof_segments_(0),
  section_header_size_(0),
  numberof_sections_(0),
  section_string_table_idx_(0)
{}


Header::Header(const Elf32_Ehdr *header):
  file_type_(static_cast<E_TYPE>(header->e_type)),
  machine_type_(static_cast<ARCH>(header->e_machine)),
  object_file_version_(static_cast<VERSION>(header->e_version)),
  entrypoint_(header->e_entry),
  program_headers_offset_(header->e_phoff),
  section_headers_offset_(header->e_shoff),
  processor_flags_(header->e_flags),
  header_size_(header->e_ehsize),
  program_header_size_(header->e_phentsize),
  numberof_segments_(header->e_phnum),
  section_header_size_(header->e_shentsize),
  numberof_sections_(header->e_shnum),
  section_string_table_idx_(header->e_shstrndx)
{
 std::copy(
     reinterpret_cast<const uint8_t*>(header->e_ident),
     reinterpret_cast<const uint8_t*>(header->e_ident) + IDENTITY::EI_NIDENT,
     std::begin(this->identity_));
}

Header::Header(const Elf64_Ehdr *header):
  file_type_(static_cast<E_TYPE>(header->e_type)),
  machine_type_(static_cast<ARCH>(header->e_machine)),
  object_file_version_(static_cast<VERSION>(header->e_version)),
  entrypoint_(header->e_entry),
  program_headers_offset_(header->e_phoff),
  section_headers_offset_(header->e_shoff),
  processor_flags_(header->e_flags),
  header_size_(header->e_ehsize),
  program_header_size_(header->e_phentsize),
  numberof_segments_(header->e_phnum),
  section_header_size_(header->e_shentsize),
  numberof_sections_(header->e_shnum),
  section_string_table_idx_(header->e_shstrndx)
{
  std::copy(
      reinterpret_cast<const uint8_t*>(header->e_ident),
      reinterpret_cast<const uint8_t*>(header->e_ident) + IDENTITY::EI_NIDENT,
      std::begin(this->identity_));
}


Header::Header(const std::vector<uint8_t>& header) {
  //TODO: Add more check
  if (header[1] == 'E' and header[2] == 'L' and header[3] == 'F') {
      uint32_t type = reinterpret_cast<const Elf32_Ehdr*>(header.data())->e_ident[IDENTITY::EI_CLASS];
      if (type == ELFCLASS32) {
        *this = Header{reinterpret_cast<const Elf32_Ehdr*>(header.data())};
      } else if(type == ELFCLASS64) {
        *this = Header{reinterpret_cast<const Elf64_Ehdr*>(header.data())};
      } else {
        throw corrupted("Incorrect header (Wrong ELFCLASS)");
      }
  } else {
    throw corrupted("Incorrect header (Wrong magic)");
  }
}

E_TYPE Header::file_type(void) const {
  return this->file_type_;
}


ARCH Header::machine_type(void) const {
  return this->machine_type_;
}

OBJECT_TYPES Header::abstract_object_type(void) const {
  try {
    return obj_elf_to_lief.at(this->file_type());
  } catch (const std::out_of_range&) {
    throw not_implemented(to_string(this->file_type()));
  }
}


std::pair<ARCHITECTURES, std::set<MODES>> Header::abstract_architecture(void) const {
  try {
    return arch_elf_to_lief.at(this->machine_type());
  } catch (const std::out_of_range&) {
    throw not_implemented(to_string(this->machine_type()));
  }
}


ENDIANNESS Header::abstract_endianness(void) const {
  try {
    return endi_elf_to_lief.at(this->identity_data());
  } catch (const std::out_of_range&) {
    throw corrupted("Invalid encoding");
  }
}


VERSION Header::object_file_version(void) const {
  return this->object_file_version_;
}


uint64_t Header::entrypoint(void) const {
  return this->entrypoint_;
}


uint64_t Header::program_headers_offset(void) const {
  return this->program_headers_offset_;
}


uint64_t Header::section_headers_offset(void) const {
  return this->section_headers_offset_;
}


uint32_t Header::processor_flag(void) const {
  return this->processor_flags_;
}


bool Header::has(ARM_EFLAGS f) const {
  if (this->machine_type() != ARCH::EM_ARM) {
    return false;
  }

  switch (f) {
    case ARM_EFLAGS::EF_ARM_EABI_VER1:
    case ARM_EFLAGS::EF_ARM_EABI_VER2:
    case ARM_EFLAGS::EF_ARM_EABI_VER3:
    case ARM_EFLAGS::EF_ARM_EABI_VER4:
    case ARM_EFLAGS::EF_ARM_EABI_VER5:
      {
        return (this->processor_flag() & ARM_EFLAGS::EF_ARM_EABIMASK) == f;
        break;
      }
    default:
      {
        return (this->processor_flag() & f) > 0;
      }
  }
}

arm_flags_list_t Header::arm_flags_list(void) const {
  arm_flags_list_t flags;

  std::copy_if(
      std::begin(arm_eflags_array),
      std::end(arm_eflags_array),
      std::inserter(flags, std::begin(flags)),
      std::bind(static_cast<bool (Header::*)(ARM_EFLAGS) const>(&Header::has), this, std::placeholders::_1));

  return flags;

}

bool Header::has(MIPS_EFLAGS f) const {
  if (this->machine_type() != ARCH::EM_MIPS) {
    return false;
  }

  if (this->machine_type() != ARCH::EM_MIPS_RS3_LE) {
    return false;
  }

  if (this->machine_type() != ARCH::EM_MIPS_X) {
    return false;
  }

  switch(f) {
    case MIPS_EFLAGS::EF_MIPS_NOREORDER:
    case MIPS_EFLAGS::EF_MIPS_PIC:
    case MIPS_EFLAGS::EF_MIPS_CPIC:
    case MIPS_EFLAGS::EF_MIPS_ABI2:
    case MIPS_EFLAGS::EF_MIPS_32BITMODE:
    case MIPS_EFLAGS::EF_MIPS_FP64:
    case MIPS_EFLAGS::EF_MIPS_NAN2008:
      {
        return (this->processor_flag() & f) > 0;
        break;
      }

    case MIPS_EFLAGS::EF_MIPS_ABI_O32:
    case MIPS_EFLAGS::EF_MIPS_ABI_O64:
    case MIPS_EFLAGS::EF_MIPS_ABI_EABI32:
    case MIPS_EFLAGS::EF_MIPS_ABI_EABI64:
      {
        return ((this->processor_flag() & MIPS_EFLAGS::EF_MIPS_ABI) & f) > 0;
        break;
      }

    case MIPS_EFLAGS::EF_MIPS_MACH_3900:
    case MIPS_EFLAGS::EF_MIPS_MACH_4010:
    case MIPS_EFLAGS::EF_MIPS_MACH_4100:
    case MIPS_EFLAGS::EF_MIPS_MACH_4650:
    case MIPS_EFLAGS::EF_MIPS_MACH_4120:
    case MIPS_EFLAGS::EF_MIPS_MACH_4111:
    case MIPS_EFLAGS::EF_MIPS_MACH_SB1:
    case MIPS_EFLAGS::EF_MIPS_MACH_OCTEON:
    case MIPS_EFLAGS::EF_MIPS_MACH_XLR:
    case MIPS_EFLAGS::EF_MIPS_MACH_OCTEON2:
    case MIPS_EFLAGS::EF_MIPS_MACH_OCTEON3:
    case MIPS_EFLAGS::EF_MIPS_MACH_5400:
    case MIPS_EFLAGS::EF_MIPS_MACH_5900:
    case MIPS_EFLAGS::EF_MIPS_MACH_5500:
    case MIPS_EFLAGS::EF_MIPS_MACH_9000:
    case MIPS_EFLAGS::EF_MIPS_MACH_LS2E:
    case MIPS_EFLAGS::EF_MIPS_MACH_LS2F:
    case MIPS_EFLAGS::EF_MIPS_MACH_LS3A:
      {
        return ((this->processor_flag() & MIPS_EFLAGS::EF_MIPS_MACH) & f) > 0;
        break;
      }


    case MIPS_EFLAGS::EF_MIPS_MICROMIPS:
    case MIPS_EFLAGS::EF_MIPS_ARCH_ASE_M16:
    case MIPS_EFLAGS::EF_MIPS_ARCH_ASE_MDMX:
      {
        return ((this->processor_flag() & MIPS_EFLAGS::EF_MIPS_ARCH_ASE) & f) > 0;
        break;
      }

    case MIPS_EFLAGS::EF_MIPS_ARCH_1:
    case MIPS_EFLAGS::EF_MIPS_ARCH_2:
    case MIPS_EFLAGS::EF_MIPS_ARCH_3:
    case MIPS_EFLAGS::EF_MIPS_ARCH_4:
    case MIPS_EFLAGS::EF_MIPS_ARCH_5:
    case MIPS_EFLAGS::EF_MIPS_ARCH_32:
    case MIPS_EFLAGS::EF_MIPS_ARCH_64:
    case MIPS_EFLAGS::EF_MIPS_ARCH_32R2:
    case MIPS_EFLAGS::EF_MIPS_ARCH_64R2:
    case MIPS_EFLAGS::EF_MIPS_ARCH_32R6:
    case MIPS_EFLAGS::EF_MIPS_ARCH_64R6:
      {
        return (this->processor_flag() & MIPS_EFLAGS::EF_MIPS_ARCH) == f;
        break;
      }

    default:
      {
        return (this->processor_flag() & f) > 0;
      }
  }


  return (this->processor_flag() & f) > 0;
}

mips_flags_list_t Header::mips_flags_list(void) const {
  mips_flags_list_t flags;

  std::copy_if(
      std::begin(mips_eflags_array),
      std::end(mips_eflags_array),
      std::inserter(flags, std::begin(flags)),
      std::bind(static_cast<bool (Header::*)(MIPS_EFLAGS) const>(&Header::has), this, std::placeholders::_1));

  return flags;

}


bool Header::has(PPC64_EFLAGS f) const {
  if (this->machine_type() != ARCH::EM_PPC64) {
    return false;
  }

  return (this->processor_flag() & f) > 0;
}

ppc64_flags_list_t Header::ppc64_flags_list(void) const {
  ppc64_flags_list_t flags;

  std::copy_if(
      std::begin(ppc64_eflags_array),
      std::end(ppc64_eflags_array),
      std::inserter(flags, std::begin(flags)),
      std::bind(static_cast<bool (Header::*)(PPC64_EFLAGS) const>(&Header::has), this, std::placeholders::_1));

  return flags;

}


bool Header::has(HEXAGON_EFLAGS f) const {
  if (this->machine_type() != ARCH::EM_HEXAGON) {
    return false;
  }

  return (this->processor_flag() & f) > 0;
}

hexagon_flags_list_t Header::hexagon_flags_list(void) const {
  hexagon_flags_list_t flags;

  std::copy_if(
      std::begin(hexagon_eflags_array),
      std::end(hexagon_eflags_array),
      std::inserter(flags, std::begin(flags)),
      std::bind(static_cast<bool (Header::*)(HEXAGON_EFLAGS) const>(&Header::has), this, std::placeholders::_1));

  return flags;

}


uint32_t Header::header_size(void) const {
  return this->header_size_;
}


uint32_t Header::program_header_size(void) const {
  return this->program_header_size_;
}


uint32_t Header::numberof_segments(void) const {
  return this->numberof_segments_;
}

uint32_t Header::section_header_size(void) const {
  return this->section_header_size_;
}

uint32_t Header::numberof_sections(void) const {
  return this->numberof_sections_;
}


uint32_t Header::section_name_table_idx(void) const {
  return this->section_string_table_idx_;
}


const Header::identity_t& Header::identity(void) const {
  return this->identity_;
}

Header::identity_t& Header::identity(void) {
  return const_cast<Header::identity_t&>(static_cast<const Header*>(this)->identity());
}

ELF_CLASS Header::identity_class(void) const {
  return static_cast<ELF_CLASS>(this->identity_[IDENTITY::EI_CLASS]);
}

ELF_DATA Header::identity_data(void) const {
  return static_cast<ELF_DATA>(this->identity_[IDENTITY::EI_DATA]);
}

VERSION Header::identity_version(void) const {
  return static_cast<VERSION>(this->identity_[IDENTITY::EI_VERSION]);
}

OS_ABI Header::identity_os_abi(void) const {
  return static_cast<OS_ABI>(this->identity_[IDENTITY::EI_OSABI]);
}

void Header::file_type(E_TYPE type) {
  this->file_type_ = type;
}


void Header::machine_type(ARCH machineType) {
  this->machine_type_ = machineType;
}


void Header::object_file_version(VERSION version) {
  this->object_file_version_ = version;
}


void Header::entrypoint(uint64_t entryPoint) {
  this->entrypoint_ = entryPoint;
}


void Header::program_headers_offset(uint64_t programHeaderOffset) {
  this->program_headers_offset_ = programHeaderOffset;
}


void Header::section_headers_offset(uint64_t sectionHeaderOffset) {
  this->section_headers_offset_ = sectionHeaderOffset;
}


void Header::processor_flag(uint32_t processorFlag) {
  this->processor_flags_ = processorFlag;
}


void Header::header_size(uint32_t headerSize) {
  this->header_size_ = headerSize;
}


void Header::program_header_size(uint32_t programHeaderSize) {
  this->program_header_size_ = programHeaderSize;
}


void Header::numberof_segments(uint32_t n) {
  this->numberof_segments_ = n;
}


void Header::section_header_size(uint32_t sizeOfSectionHeaderEntries) {
  this->section_header_size_ = sizeOfSectionHeaderEntries;
}


void Header::numberof_sections(uint32_t n) {
  this->numberof_sections_ = n;
}


void Header::section_name_table_idx(uint32_t sectionNameStringTableIdx) {
  this->section_string_table_idx_ = sectionNameStringTableIdx;
}


void Header::identity(const std::string& identity) {
  std::copy(
      std::begin(identity),
      std::end(identity),
      std::begin(this->identity_));
}

void Header::identity(const Header::identity_t& identity) {
  std::copy(
      std::begin(identity),
      std::end(identity),
      std::begin(this->identity_));
}

void Header::identity_class(ELF_CLASS i_class) {
  this->identity_[IDENTITY::EI_CLASS] = static_cast<uint8_t>(i_class);
}

void Header::identity_data(ELF_DATA data) {
  this->identity_[IDENTITY::EI_DATA] = static_cast<uint8_t>(data);
}

void Header::identity_version(VERSION version) {
  this->identity_[IDENTITY::EI_VERSION] = static_cast<uint8_t>(version);
}

void Header::identity_os_abi(OS_ABI osabi) {
  this->identity_[IDENTITY::EI_OSABI] = static_cast<uint8_t>(osabi);
}


void Header::accept(LIEF::Visitor& visitor) const {
  visitor.visit(this->entrypoint());
  visitor.visit(this->file_type());
  visitor.visit(this->machine_type());
  visitor.visit(this->object_file_version());
  visitor.visit(this->entrypoint());
  visitor.visit(this->program_headers_offset());
  visitor.visit(this->section_headers_offset());
  visitor.visit(this->processor_flag());
  visitor.visit(this->header_size());
  visitor.visit(this->program_header_size());
  visitor.visit(this->numberof_segments());
  visitor.visit(this->section_header_size());
  visitor.visit(this->section_name_table_idx());
  visitor.visit(this->identity_class());
  visitor.visit(this->identity_data());
  visitor.visit(this->identity_version());
  visitor.visit(this->identity_os_abi());
}

bool Header::operator==(const Header& rhs) const {
  size_t hash_lhs = Hash::hash(*this);
  size_t hash_rhs = Hash::hash(rhs);
  return hash_lhs == hash_rhs;
}

bool Header::operator!=(const Header& rhs) const {
  return not (*this == rhs);
}



std::ostream& operator<<(std::ostream& os, const Header& hdr)
{
  const Header::identity_t& identity = const_cast<Header*>(&hdr)->identity();
  std::stringstream ss;
  ss << std::hex;
  ss << static_cast<uint32_t>(identity[IDENTITY::EI_MAG0]) << " ";
  ss << static_cast<uint32_t>(identity[IDENTITY::EI_MAG1]) << " ";
  ss << static_cast<uint32_t>(identity[IDENTITY::EI_MAG2]) << " ";
  ss << static_cast<uint32_t>(identity[IDENTITY::EI_MAG3]) << " ";
  const std::string& ident_magic = ss.str();

  std::string processor_flags_str = "";

  if (hdr.machine_type() == ARCH::EM_ARM) {
    const arm_flags_list_t& flags = hdr.arm_flags_list();
    processor_flags_str = std::accumulate(
     std::begin(flags),
     std::end(flags), std::string{},
     [] (const std::string& a, ARM_EFLAGS b) {
         return a.empty() ? to_string(b) : a + " " + to_string(b);
     });
  }


  if (hdr.machine_type() == ARCH::EM_PPC64) {
    const ppc64_flags_list_t& flags = hdr.ppc64_flags_list();
    processor_flags_str = std::accumulate(
     std::begin(flags),
     std::end(flags), std::string{},
     [] (const std::string& a, PPC64_EFLAGS b) {
         return a.empty() ? to_string(b) : a + " " + to_string(b);
     });
  }

  if (hdr.machine_type() == ARCH::EM_HEXAGON) {
    const hexagon_flags_list_t& flags = hdr.hexagon_flags_list();
    processor_flags_str = std::accumulate(
     std::begin(flags),
     std::end(flags), std::string{},
     [] (const std::string& a, HEXAGON_EFLAGS b) {
         return a.empty() ? to_string(b) : a + " " + to_string(b);
     });
  }


  if (hdr.machine_type() == ARCH::EM_MIPS or
      hdr.machine_type() == ARCH::EM_MIPS_RS3_LE or
      hdr.machine_type() == ARCH::EM_MIPS_X) {
    const mips_flags_list_t& flags = hdr.mips_flags_list();
    processor_flags_str = std::accumulate(
     std::begin(flags),
     std::end(flags), std::string{},
     [] (const std::string& a, MIPS_EFLAGS b) {
         return a.empty() ? to_string(b) : a + " " + to_string(b);
     });
  }

  os << std::hex << std::left;
  os << std::setw(33) << std::setfill(' ') << "Magic:"                     << ident_magic << std::endl;
  os << std::setw(33) << std::setfill(' ') << "Class"                      << to_string(hdr.identity_class()) << std::endl;
  os << std::setw(33) << std::setfill(' ') << "Endianness:"                << to_string(hdr.identity_data()) << std::endl;
  os << std::setw(33) << std::setfill(' ') << "Version:"                   << to_string(hdr.identity_version()) << std::endl;
  os << std::setw(33) << std::setfill(' ') << "OS/ABI:"                    << to_string(hdr.identity_os_abi()) << std::endl;
  os << std::setw(33) << std::setfill(' ') << "Machine type:"              << to_string(hdr.machine_type()) << std::endl;
  os << std::setw(33) << std::setfill(' ') << "File type:"                 << to_string(hdr.file_type()) << std::endl;
  os << std::setw(33) << std::setfill(' ') << "Object file version:"       << to_string(hdr.object_file_version()) << std::endl;
  os << std::setw(33) << std::setfill(' ') << "Entry Point:"               << "0x" << hdr.entrypoint() << std::endl;
  os << std::setw(33) << std::setfill(' ') << "Program header offset:"     << "0x" << hdr.program_headers_offset() << std::endl;
  os << std::setw(33) << std::setfill(' ') << "Section header offset:"     << hdr.section_headers_offset() << std::endl;
  os << std::setw(33) << std::setfill(' ') << "Processor Flag"             << hdr.processor_flag() << " " << processor_flags_str << std::endl;
  os << std::setw(33) << std::setfill(' ') << "Header size:"               << hdr.header_size() << std::endl;
  os << std::setw(33) << std::setfill(' ') << "Size of program header :"   << hdr.program_header_size() << std::endl;
  os << std::setw(33) << std::setfill(' ') << "Number of program header:"  << hdr.numberof_segments() << std::endl;
  os << std::setw(33) << std::setfill(' ') << "Size of section header:"    << hdr.section_header_size() << std::endl;
  os << std::setw(33) << std::setfill(' ') << "Number of section headers:" << hdr.numberof_sections() << std::endl;
  os << std::setw(33) << std::setfill(' ') << "Section Name Table idx:"    << hdr.section_name_table_idx() << std::endl;

  return os;
}
}
}
