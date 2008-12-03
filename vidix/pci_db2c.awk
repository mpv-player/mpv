# This file converts given pci.db to "C" source and header files
# For latest version of pci ids see: http://pciids.sf.net
# Copyright 2002 Nick Kurshev
#
# Usage: awk -f pci_db2c.awk pci.db
#
# Tested with Gawk v 3.0.x and Mawk 1.3.3
# But it should work with standard Awk implementations (hopefully).
# (Nobody tested it with Nawk, but it should work, too).
#
# This file is part of MPlayer.
#
# MPlayer is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# MPlayer is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with MPlayer; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

BEGIN {

    if(ARGC != 3) {
# check for arguments:
	print "Usage awk -f pci_db2c.awk pci.db (and make sure pci.db file exists first)";
	exit(1);
    }
    in_file = ARGV[1];
    with_pci_db = ARGV[2];
    vendor_file = "vidix/pci_vendors.h";
    ids_file = "vidix/pci_ids.h"
    name_file = "vidix/pci_names.c"
    name_h_file = "vidix/pci_names.h"
    dev_ids_file = "vidix/pci_dev_ids.c"
    line=0;
# print out head lines
    print_head(vendor_file);
    print_head(ids_file);
    print_head(name_file);
    print_head(name_h_file);
    print_head(dev_ids_file);
    print_includes(dev_ids_file);
    print "#ifndef MPLAYER_PCI_VENDORS_H" >vendor_file
    print "#define MPLAYER_PCI_VENDORS_H">vendor_file
    print "" >vendor_file
    print "#ifndef MPLAYER_PCI_IDS_H" >ids_file
    print "#define MPLAYER_PCI_IDS_H">ids_file
    print "" >ids_file
    print "#include \"pci_vendors.h\"">ids_file
    print "" >ids_file

    print "#ifndef MPLAYER_PCI_NAMES_H" >name_h_file
    print "#define MPLAYER_PCI_NAMES_H">name_h_file
    print "" >name_h_file
    print_name_struct(name_h_file);
    print "#include <stddef.h>">name_file
    print "#include \"pci_names.h\"">name_file
    if (with_pci_db) {
    print "#include \"pci_dev_ids.c\"">name_file
    print "">name_file
    print "static struct vendor_id_s vendor_ids[] = {">name_file
    }
    first_pass=1;
    init_name_db();
    while(getline <in_file) 
    {
# count up lines
	line++;
	n=split($0, field, "[\t]");
	name_field = kill_double_quoting(field[3])
	if(field[1] == "v" && length(field[3])>0 && field[4] == "0")
	{
		init_device_db()
		svend_name = get_short_vendor_name(field[3])
		printf("#define VENDOR_%s\t", svend_name) >vendor_file;
		if(length(svend_name) < 9) printf("\t") >vendor_file;
		printf("0x%s /*%s*/\n",field[2], name_field) >vendor_file;
		if (with_pci_db) printf("{ 0x%s, \"%s\", dev_lst_%s },\n",field[2], name_field, field[2]) >name_file;
		printf("/* Vendor: %s: %s */\n", field[2], name_field) > ids_file
		if(first_pass == 1) { first_pass=0; }
		else	{ print "{ 0xFFFF,  NULL }\n};" >dev_ids_file; }
		printf("static const struct device_id_s dev_lst_%s[]={\n", field[2])>dev_ids_file
	}
	if(field[1] == "d" && length(field[3])>0 && field[4] == "0")
	{
		sdev_name = get_short_device_name(field[3])
		full_name = sprintf("#define DEVICE_%s_%s", svend_name, sdev_name);
		printf("%s\t", full_name) >ids_file
		if(length(full_name) < 9) printf("\t") >ids_file;
		if(length(full_name) < 17) printf("\t") >ids_file;
		if(length(full_name) < 25) printf("\t") >ids_file;
		if(length(full_name) < 32) printf("\t") >ids_file;
		if(length(full_name) < 40) printf("\t") >ids_file;
		if(length(full_name) < 48) printf("\t") >ids_file;
		printf("0x%s /*%s*/\n", substr(field[2], 5), name_field) >ids_file
		printf("{ 0x%s, \"%s\" },\n", substr(field[2], 5), name_field) >dev_ids_file
	}
	if(field[1] == "s" && length(field[3])>0 && field[4] == "0")
	{
		subdev_name = get_short_subdevice_name(field[3])
		full_name = sprintf("#define SUBDEVICE_%s_%s", svend_name, subdev_name)
		printf("\t%s\t", full_name) >ids_file
		if(length(full_name) < 9) printf("\t") >ids_file;
		if(length(full_name) < 17) printf("\t") >ids_file;
		if(length(full_name) < 25) printf("\t") >ids_file;
		if(length(full_name) < 32) printf("\t") >ids_file;
		if(length(full_name) < 40) printf("\t") >ids_file;
		printf("0x%s /*%s*/\n", substr(field[2], 9), name_field) >ids_file
	}
    }
    #print "Total lines parsed:", line;
    print "">vendor_file
    print "#endif /* MPLAYER_PCI_VENDORS_H */">vendor_file
    print "">ids_file
    print "#endif /* MPLAYER_PCI_IDS_H */">ids_file
    print "">name_h_file
    print "#endif /* MPLAYER_PCI_NAMES_H */">name_h_file
    if (with_pci_db) print "};">name_file
    print "{ 0xFFFF,  NULL }" >dev_ids_file;
    print "};">dev_ids_file
    print_func_bodies(name_file);
}

function print_includes(out_file)
{
    print "#include <stdlib.h>" >out_file;
    print "#include \"pci_names.h\"" >out_file;
    return;
}

function print_head( out_file)
{
    print "/*" >out_file;
    printf(" * File: %s\n", out_file) >out_file;
    printf(" * This file was generated automatically. Don't modify it.\n") >out_file;
    print "*/" >out_file;
    return;
}

function print_name_struct(out_file)
{
   print "">out_file
   print "struct device_id_s" >out_file
   print "{" >out_file
   print "\tunsigned short\tid;" >out_file
   print "\tconst char *\tname;" >out_file
   print "};" >out_file
   print "">out_file
   print "struct vendor_id_s" >out_file
   print "{" >out_file
   print "\tunsigned short\tid;" >out_file
   print "\tconst char *\tname;" >out_file
   print "\tconst struct device_id_s *\tdev_list;" >out_file
   print "};" >out_file
   print "const char *pci_vendor_name(unsigned short id);">out_file
   print "const char *pci_device_name(unsigned short vendor_id, unsigned short device_id);">out_file
   print "">out_file
   return
}

function print_func_bodies(out_file)
{
   print "">out_file
   print "const char *pci_vendor_name(unsigned short id)" >out_file
   print "{" >out_file
   if (with_pci_db) {
   print "  unsigned i;" >out_file
   print "  for(i=0;i<sizeof(vendor_ids)/sizeof(struct vendor_id_s);i++)">out_file
   print "  {" >out_file
   print "\tif(vendor_ids[i].id == id) return vendor_ids[i].name;" >out_file
   print "  }" >out_file
   }
   print "  return NULL;" >out_file
   print "}">out_file
   print "" >out_file
   print "const char *pci_device_name(unsigned short vendor_id, unsigned short device_id)" >out_file
   print "{" >out_file
   if (with_pci_db) {
   print "  unsigned i, j;" >out_file
   print "  for(i=0;i<sizeof(vendor_ids)/sizeof(struct vendor_id_s);i++)">out_file
   print "  {" >out_file
   print "\tif(vendor_ids[i].id == vendor_id)" >out_file
   print "\t{" >out_file
   print "\t  j=0;" >out_file
   print "\t  while(vendor_ids[i].dev_list[j].id != 0xFFFF)" >out_file
   print "\t  {">out_file
   print "\t\tif(vendor_ids[i].dev_list[j].id == device_id) return vendor_ids[i].dev_list[j].name;">out_file
   print "\t\tj++;">out_file
   print "\t  };">out_file
   print "\t  break;" >out_file
   print "\t}" >out_file
   print "  }" >out_file
   }
   print "  return NULL;">out_file
   print "}">out_file
   return
}

function kill_double_quoting(fld)
{
  n=split(fld,phrases, "[\"]");
  new_fld = phrases[1]
  for(i=2;i<=n;i++) new_fld = sprintf("%s\\\"%s", new_fld, phrases[i])
  return new_fld
}

function init_name_db()
{
  vendor_names[1]=""
}

function init_device_db()
{
#  delete device_names
  for( i in device_names ) delete device_names[i];
  device_names[1]=""
#  delete subdevice_names
  for( i in subdevice_names ) delete subdevice_names[i];
  subdevice_names[1] = ""
}

function get_short_vendor_name(from)
{
  n=split(from, name, "[ ]");
  new_name = toupper(name[1]);
  if(length(new_name)<3) new_name = sprintf("%s_%s", new_name, toupper(name[2]));
  n=split(new_name, name, "[^0-9A-Za-z]");
  svendor = name[1];
  for(i=2;i<=n;i++) svendor=sprintf("%s%s%s", svendor, length(name[i])?"_":"", name[i]);
  new_name = svendor;
  vend_suffix = 2;
# check for unique 
  while(new_name in vendor_names) 
  {
    new_name = sprintf("%s%u", svendor, vend_suffix)
    vend_suffix = vend_suffix + 1;
  }
# Add new name in array of vendor's names
  vendor_names[new_name] = new_name
  return new_name;
}

function get_short_device_name(from_name)
{
  n=split(from_name, name, "[ ]");
  new_name = toupper(name[1]);
  if(length(name[2])) new_name = sprintf("%s_%s", new_name, toupper(name[2]));
  if(length(name[3])) new_name = sprintf("%s_%s", new_name, toupper(name[3]));
  n=split(new_name, name, "[^0-9A-Za-z]");
  sdevice = name[1];
  for(i=2;i<=n;i++) sdevice=sprintf("%s%s%s", sdevice, length(name[i])?"_":"", name[i]);
  new_name = sdevice;
  dev_suffix = 2;
# check for unique 
  while(new_name in device_names) 
  {
    new_name = sprintf("%s%u", sdevice, dev_suffix)
    dev_suffix = dev_suffix + 1;
  }
# Add new name in array of device names
  device_names[new_name] = new_name
  return new_name;
}

function get_short_subdevice_name(from_name)
{
  n=split(from_name, name, "[ ]");
  new_name = toupper(name[1]);
  if(length(name[2])) new_name = sprintf("%s_%s", new_name, toupper(name[2]));
  if(length(name[3])) new_name = sprintf("%s_%s", new_name, toupper(name[3]));
  n=split(new_name, name, "[^0-9A-Za-z]");
  ssdevice = name[1];
  for(i=2;i<=n;i++) ssdevice=sprintf("%s%s%s", ssdevice, length(name[i])?"_":"", name[i]);
  new_name = ssdevice;
  sdev_suffix = 2;
# check for unique 
  while(new_name in subdevice_names) 
  {
    new_name = sprintf("%s%u", ssdevice, sdev_suffix)
    sdev_suffix = sdev_suffix + 1;
  }
# Add new name in array of subdevice names
  subdevice_names[new_name] = new_name
  return new_name;
}
