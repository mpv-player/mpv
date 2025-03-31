/*
 *      Copyright (C) 2010-2021 Hendrik Leppkes
 *      http://www.1f0.de
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *  Initial design and concept by Gabest and the MPC-HC Team, copyright under GPLv2
 */

#include "stdafx.h"
#include "DShowUtil.h"

#include <string>
#include <regex>
#include <algorithm>

static struct
{
    LPCSTR name, iso6392, iso6391, iso6392_2;
    LCID lcid;
} s_isolangs[] = // TODO : fill LCID !!!
    {
        // Based on ISO-639-2, sorted by primary language code. Some manual additions of deprecated tags.
        {"Afar", "aar", "aa"},
        {"Abkhazian", "abk", "ab"},
        {"Achinese", "ace", nullptr},
        {"Acoli", "ach", nullptr},
        {"Adangme", "ada", nullptr},
        {"Adyghe", "ady", nullptr},
        {"Afro-Asiatic (Other)", "afa", nullptr},
        {"Afrihili", "afh", nullptr},
        {"Afrikaans", "afr", "af", nullptr, MAKELCID(MAKELANGID(LANG_AFRIKAANS, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Ainu", "ain", nullptr},
        {"Akan", "aka", "ak"},
        {"Akkadian", "akk", nullptr},
        {"Albanian", "sqi", "sq", "alb", MAKELCID(MAKELANGID(LANG_ALBANIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Aleut", "ale", nullptr},
        {"Algonquian languages", "alg", nullptr},
        {"Southern Altai", "alt", nullptr},
        {"Amharic", "amh", "am"},
        {"English, Old (ca.450-1100)", "ang", nullptr},
        {"Angika", "anp", nullptr},
        {"Apache languages", "apa", nullptr},
        {"Arabic", "ara", "ar", nullptr, MAKELCID(MAKELANGID(LANG_ARABIC, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Aramaic", "arc", nullptr},
        {"Aragonese", "arg", "an"},
        {"Armenian", "arm", "hy", "hye", MAKELCID(MAKELANGID(LANG_ARMENIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Mapudungun", "arn", nullptr},
        {"Arapaho", "arp", nullptr},
        {"Artificial (Other)", "art", nullptr},
        {"Arawak", "arw", nullptr},
        {"Assamese", "asm", "as", nullptr, MAKELCID(MAKELANGID(LANG_ASSAMESE, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Asturian; Bable", "ast", nullptr},
        {"Athapascan languages", "ath", nullptr},
        {"Australian languages", "aus", nullptr},
        {"Avaric", "ava", "av"},
        {"Avestan", "ave", "ae"},
        {"Awadhi", "awa", nullptr},
        {"Aymara", "aym", "ay"},
        {"Azerbaijani", "aze", "az", nullptr, MAKELCID(MAKELANGID(LANG_AZERI, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Banda", "bad", nullptr},
        {"Bamileke languages", "bai", nullptr},
        {"Bashkir", "bak", "ba", nullptr, MAKELCID(MAKELANGID(LANG_BASHKIR, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Baluchi", "bal", nullptr},
        {"Bambara", "bam", "bm"},
        {"Balinese", "ban", nullptr},
        {"Basque", "baq", "eu", "eus", MAKELCID(MAKELANGID(LANG_BASQUE, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Basa", "bas", nullptr},
        {"Baltic (Other)", "bat", nullptr},
        {"Beja", "bej", nullptr},
        {"Belarusian", "bel", "be", nullptr, MAKELCID(MAKELANGID(LANG_BELARUSIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Bemba", "bem", nullptr},
        {"Bengali", "ben", "bn", nullptr, MAKELCID(MAKELANGID(LANG_BENGALI, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Berber (Other)", "ber", nullptr},
        {"Bhojpuri", "bho", nullptr},
        {"Bihari", "bih", "bh"},
        {"Bikol", "bik", nullptr},
        {"Bini", "bin", nullptr},
        {"Bislama", "bis", "bi"},
        {"Siksika", "bla", nullptr},
        {"Bantu (Other)", "bnt", nullptr},
        {"Bosnian", "bos", "bs"},
        {"Braj", "bra", nullptr},
        {"Breton", "bre", "br", nullptr, MAKELCID(MAKELANGID(LANG_BRETON, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Batak (Indonesia)", "btk", nullptr},
        {"Buriat", "bua", nullptr},
        {"Buginese", "bug", nullptr},
        {"Bulgarian", "bul", "bg", nullptr, MAKELCID(MAKELANGID(LANG_BULGARIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Burmese", "bur", "my", "mya"},
        {"Blin", "byn", nullptr},
        {"Caddo", "cad", nullptr},
        {"Central American Indian (Other)", "cai", nullptr},
        {"Carib", "car", nullptr},
        {"Catalan", "cat", "ca", nullptr, MAKELCID(MAKELANGID(LANG_CATALAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Caucasian (Other)", "cau", nullptr},
        {"Cebuano", "ceb", nullptr},
        {"Celtic (Other)", "cel", nullptr},
        {"Chamorro", "cha", "ch"},
        {"Chibcha", "chb", nullptr},
        {"Chechen", "che", "ce"},
        {"Chagatai", "chg", nullptr},
        {"Chinese", "chi", "zh", "zho", MAKELCID(MAKELANGID(LANG_CHINESE, SUBLANG_NEUTRAL), SORT_DEFAULT)},
        {"Chuukese", "chk", nullptr},
        {"Mari", "chm", nullptr},
        {"Chinook jargon", "chn", nullptr},
        {"Choctaw", "cho", nullptr},
        {"Chipewyan", "chp", nullptr},
        {"Cherokee", "chr", nullptr},
        {"Church Slavic", "chu", "cu"},
        {"Chuvash", "chv", "cv"},
        {"Cheyenne", "chy", nullptr},
        {"Chamic languages", "cmc", nullptr},
        {"Montenegrin", "cnr", nullptr},
        {"Coptic", "cop", nullptr},
        {"Cornish", "cor", "kw"},
        {"Corsican", "cos", "co", nullptr, MAKELCID(MAKELANGID(LANG_CORSICAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Creoles and pidgins, English-based", "cpe", nullptr},
        {"Creoles and pidgins, French-based", "cpf", nullptr},
        {"Creoles and pidgins, Portuguese-based", "cpp", nullptr},
        {"Cree", "cre", "cr"},
        {"Crimean Turkish", "crh", nullptr},
        {"Creoles and pidgins (Other)", "crp", nullptr},
        {"Kashubian", "csb", nullptr},
        {"Cushitic (Other)", "cus", nullptr},
        {"Czech", "cze", "cs", "ces", MAKELCID(MAKELANGID(LANG_CZECH, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Dakota", "dak", nullptr},
        {"Danish", "dan", "da", nullptr, MAKELCID(MAKELANGID(LANG_DANISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Dargwa", "dar", nullptr},
        {"Dayak", "day", nullptr},
        {"Delaware", "del", nullptr},
        {"Slave (Athapascan)", "den", nullptr},
        {"Dogrib", "dgr", nullptr},
        {"Dinka", "din", nullptr},
        {"Divehi", "div", "dv", nullptr, MAKELCID(MAKELANGID(LANG_DIVEHI, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Dogri", "doi", nullptr},
        {"Dravidian (Other)", "dra", nullptr},
        {"Lower Sorbian", "dsb", nullptr},
        {"Duala", "dua", nullptr},
        {"Dutch, Middle (ca. 1050-1350)", "dum", nullptr},
        {"Dutch", "dut", "nl", "nld", MAKELCID(MAKELANGID(LANG_DUTCH, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Dyula", "dyu", nullptr},
        {"Dzongkha", "dzo", "dz"},
        {"Efik", "efi", nullptr},
        {"Egyptian (Ancient)", "egy", nullptr},
        {"Ekajuk", "eka", nullptr},
        {"Elamite", "elx", nullptr},
        {"English", "eng", "en", nullptr, MAKELCID(MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"English, Middle (1100-1500)", "enm", nullptr},
        {"Esperanto", "epo", "eo"},
        {"Estonian", "est", "et", nullptr, MAKELCID(MAKELANGID(LANG_ESTONIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Ewe", "ewe", "ee"},
        {"Ewondo", "ewo", nullptr},
        {"Fang", "fan", nullptr},
        {"Faroese", "fao", "fo", nullptr, MAKELCID(MAKELANGID(LANG_FAEROESE, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Fanti", "fat", nullptr},
        {"Fijian", "fij", "fj"},
        {"Filipino", "fil", nullptr},
        {"Finnish", "fin", "fi", nullptr, MAKELCID(MAKELANGID(LANG_FINNISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Finno-Ugrian (Other)", "fiu", nullptr},
        {"Fon", "fon", nullptr},
        {"French", "fre", "fr", "fra", MAKELCID(MAKELANGID(LANG_FRENCH, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"French, Middle (ca.1400-1600)", "frm", nullptr},
        {"French, Old (842-ca.1400)", "fro", nullptr},
        {"Northern Frisian", "frr", nullptr},
        {"Eastern Frisian", "frs", nullptr},
        {"Frisian", "fry", "fy", nullptr, MAKELCID(MAKELANGID(LANG_FRISIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Fulah", "ful", "ff"},
        {"Friulian", "fur", nullptr},
        {"Ga", "gaa", nullptr},
        {"Gayo", "gay", nullptr},
        {"Gbaya", "gba", nullptr},
        {"Germanic (Other)", "gem", nullptr},
        {"Georgian", "geo", "ka", "kat", MAKELCID(MAKELANGID(LANG_GEORGIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"German", "ger", "de", "deu", MAKELCID(MAKELANGID(LANG_GERMAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Geez", "gez", nullptr},
        {"Gilbertese", "gil", nullptr},
        {"Gaelic; Scottish Gaelic", "gla", "gd"},
        {"Irish", "gle", "ga", nullptr, MAKELCID(MAKELANGID(LANG_IRISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Galician", "glg", "gl", nullptr, MAKELCID(MAKELANGID(LANG_GALICIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Manx", "glv", "gv"},
        {"German, Middle High (ca.1050-1500)", "gmh", nullptr},
        {"German, Old High (ca.750-1050)", "goh", nullptr},
        {"Gondi", "gon", nullptr},
        {"Gorontalo", "gor", nullptr},
        {"Gothic", "got", nullptr},
        {"Grebo", "grb", nullptr},
        {"Ancient Greek", "grc", nullptr},
        {"Greek", "gre", "el", "ell", MAKELCID(MAKELANGID(LANG_GREEK, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Guarani", "grn", "gn"},
        {"Swiss German", "gsw", nullptr},
        {"Gujarati", "guj", "gu", nullptr, MAKELCID(MAKELANGID(LANG_GUJARATI, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Gwich´in", "gwi", nullptr},
        {"Haida", "hai", nullptr},
        {"Haitian", "hat", "ht"},
        {"Hausa", "hau", "ha", nullptr, MAKELCID(MAKELANGID(LANG_HAUSA, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Hawaiian", "haw", nullptr},
        {"Hebrew", "heb", "he", nullptr, MAKELCID(MAKELANGID(LANG_HEBREW, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Herero", "her", "hz"},
        {"Hiligaynon", "hil", nullptr},
        {"Himachali", "him", nullptr},
        {"Hindi", "hin", "hi", nullptr, MAKELCID(MAKELANGID(LANG_HINDI, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Hittite", "hit", nullptr},
        {"Hmong", "hmn", nullptr},
        {"Hiri Motu", "hmo", "ho"},
        {"Croatian", "hrv", "hr", "scr", MAKELCID(MAKELANGID(LANG_CROATIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Upper Sorbian", "hsb", nullptr},
        {"Hungarian", "hun", "hu", nullptr, MAKELCID(MAKELANGID(LANG_HUNGARIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Hupa", "hup", nullptr},
        {"Iban", "iba", nullptr},
        {"Igbo", "ibo", "ig", nullptr, MAKELCID(MAKELANGID(LANG_IGBO, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Icelandic", "ice", "is", "isl", MAKELCID(MAKELANGID(LANG_ICELANDIC, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Ido", "ido", "io"},
        {"Sichuan Yi", "iii", "ii"},
        {"Ijo", "ijo", nullptr},
        {"Inuktitut", "iku", "iu", nullptr, MAKELCID(MAKELANGID(LANG_INUKTITUT, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Interlingue", "ile", "ie"},
        {"Iloko", "ilo", nullptr},
        {"Interlingua", "ina", "ia"},
        {"Indic (Other)", "inc", nullptr},
        {"Indonesian", "ind", "id", nullptr, MAKELCID(MAKELANGID(LANG_INDONESIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Indo-European (Other)", "ine", nullptr},
        {"Ingush", "inh", nullptr},
        {"Inupiaq", "ipk", "ik"},
        {"Iranian (Other)", "ira", nullptr},
        {"Iroquoian languages", "iro", nullptr},
        {"Italian", "ita", "it", nullptr, MAKELCID(MAKELANGID(LANG_ITALIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Javanese", "jav", "jv"},
        {"Lojban", "jbo", nullptr},
        {"Japanese", "jpn", "ja", nullptr, MAKELCID(MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Judeo-Persian", "jpr", nullptr},
        {"Judeo-Arabic", "jrb", nullptr},
        {"Kara-Kalpak", "kaa", nullptr},
        {"Kabyle", "kab", nullptr},
        {"Kachin", "kac", nullptr},
        {"Greenlandic; Kalaallisut", "kal", "kl", nullptr, MAKELCID(MAKELANGID(LANG_GREENLANDIC, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Kamba", "kam", nullptr},
        {"Kannada", "kan", "kn", nullptr, MAKELCID(MAKELANGID(LANG_KANNADA, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Karen", "kar", nullptr},
        {"Kashmiri", "kas", "ks", nullptr, MAKELCID(MAKELANGID(LANG_KASHMIRI, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Kanuri", "kau", "kr"},
        {"Kawi", "kaw", nullptr},
        {"Kazakh", "kaz", "kk", nullptr, MAKELCID(MAKELANGID(LANG_KAZAK, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Kabardian", "kbd", nullptr},
        {"Khasi", "kha", nullptr},
        {"Khoisan (Other)", "khi", nullptr},
        {"Khmer", "khm", "km", nullptr, MAKELCID(MAKELANGID(LANG_KHMER, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Khotanese", "kho", nullptr},
        {"Kikuyu; Gikuyu", "kik", "ki"},
        {"Kinyarwanda", "kin", "rw", nullptr, MAKELCID(MAKELANGID(LANG_KINYARWANDA, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Kirghiz", "kir", "ky"},
        {"Kimbundu", "kmb", nullptr},
        {"Konkani", "kok", nullptr, nullptr, MAKELCID(MAKELANGID(LANG_KONKANI, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Komi", "kom", "kv"},
        {"Kongo", "kon", "kg"},
        {"Korean", "kor", "ko", nullptr, MAKELCID(MAKELANGID(LANG_KOREAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Kosraean", "kos", nullptr},
        {"Kpelle", "kpe", nullptr},
        {"Karachay-Balkar", "krc", nullptr},
        {"Karelian", "krl", nullptr},
        {"Kru", "kro", nullptr},
        {"Kurukh", "kru", nullptr},
        {"Kwanyama, Kuanyama", "kua", "kj"},
        {"Kumyk", "kum", nullptr},
        {"Kurdish", "kur", "ku"},
        {"Kutenai", "kut", nullptr},
        {"Ladino", "lad", nullptr},
        {"Lahnda", "lah", nullptr},
        {"Lamba", "lam", nullptr},
        {"Lao", "lao", "lo", nullptr, MAKELCID(MAKELANGID(LANG_LAO, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Latin", "lat", "la"},
        {"Latvian", "lav", "lv", nullptr, MAKELCID(MAKELANGID(LANG_LATVIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Lezghian", "lez", nullptr},
        {"Limburgan; Limburger; Limburgish", "lim", "li"},
        {"Lingala", "lin", "ln"},
        {"Lithuanian", "lit", "lt", nullptr, MAKELCID(MAKELANGID(LANG_LITHUANIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Mongo", "lol", nullptr},
        {"Lozi", "loz", nullptr},
        {"Luxembourgish; Letzeburgesch", "ltz", "lb", nullptr, MAKELCID(MAKELANGID(LANG_LUXEMBOURGISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Luba-Lulua", "lua", nullptr},
        {"Luba-Katanga", "lub", "lu"},
        {"Ganda", "lug", "lg"},
        {"Luiseno", "lui", nullptr},
        {"Lunda", "lun", nullptr},
        {"Luo (Kenya and Tanzania)", "luo", nullptr},
        {"Lushai", "lus", nullptr},
        {"Macedonian", "mac", "mk", "mkd", MAKELCID(MAKELANGID(LANG_MACEDONIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Madurese", "mad", nullptr},
        {"Magahi", "mag", nullptr},
        {"Marshallese", "mah", "mh"},
        {"Maithili", "mai", nullptr},
        {"Makasar", "mak", nullptr},
        {"Malayalam", "mal", "ml", nullptr, MAKELCID(MAKELANGID(LANG_MALAYALAM, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Mandingo", "man", nullptr},
        {"Maori", "mao", "mi", "mri", MAKELCID(MAKELANGID(LANG_MAORI, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Austronesian (Other)", "map", nullptr},
        {"Marathi", "mar", "mr", nullptr, MAKELCID(MAKELANGID(LANG_MARATHI, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Masai", "mas", nullptr},
        {"Malay", "may", "ms", "msa", MAKELCID(MAKELANGID(LANG_MALAY, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Moksha", "mdf", nullptr},
        {"Mandar", "mdr", nullptr},
        {"Mende", "men", nullptr},
        {"Irish, Middle (900-1200)", "mga", nullptr},
        {"Micmac", "mic", nullptr},
        {"Minangkabau", "min", nullptr},
        {"Miscellaneous languages", "mis", nullptr},
        {"Mon-Khmer (Other)", "mkh", nullptr},
        {"Malagasy", "mlg", "mg"},
        {"Maltese", "mlt", "mt", nullptr, MAKELCID(MAKELANGID(LANG_MALTESE, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Manchu", "mnc", nullptr},
        {"Manipuri", "mni", nullptr, nullptr, MAKELCID(MAKELANGID(LANG_MANIPURI, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Manobo languages", "mno", nullptr},
        {"Mohawk", "moh", nullptr, nullptr, MAKELCID(MAKELANGID(LANG_MOHAWK, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Moldavian", "mol", "mo"}, // deprecated
        {"Mongolian", "mon", "mn", nullptr, MAKELCID(MAKELANGID(LANG_MONGOLIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Mossi", "mos", nullptr},
        {"Multiple languages", "mul", nullptr},
        {"Munda languages", "mun", nullptr},
        {"Creek", "mus", nullptr},
        {"Mirandese", "mwl", nullptr},
        {"Marwari", "mwr", nullptr},
        {"Mayan languages", "myn", nullptr},
        {"Erzya", "myv", nullptr},
        {"Nahuatl", "nah", nullptr},
        {"North American Indian (Other)", "nai", nullptr},
        {"Neapolitan", "nap", nullptr},
        {"Nauru", "nau", "na"},
        {"Navaho, Navajo", "nav", "nv"},
        {"Ndebele, South", "nbl", "nr"},
        {"Ndebele, North", "nde", "nd"},
        {"Ndonga", "ndo", "ng"},
        {"Low German; Low Saxon", "nds", nullptr},
        {"Nepali", "nep", "ne", nullptr, MAKELCID(MAKELANGID(LANG_NEPALI, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Newari", "new", nullptr},
        {"Nias", "nia", nullptr},
        {"Niger-Kordofanian (Other)", "nic", nullptr},
        {"Niuean", "niu", nullptr},
        {"Norwegian Nynorsk", "nno", "nn"},
        {"Norwegian Bokmål", "nob", "nb"},
        {"Nogai", "nog", nullptr},
        {"Norse, Old", "non", nullptr},
        {"Norwegian", "nor", "no", nullptr, MAKELCID(MAKELANGID(LANG_NORWEGIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"N'Ko", "nqo", nullptr},
        {"Pedi; Sepedi; Northern Sotho", "nso", nullptr, nullptr, MAKELCID(MAKELANGID(LANG_SOTHO, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Nubian languages", "nub", nullptr},
        {"Classical Newari", "nwc", nullptr},
        {"Nyanja; Chichewa; Chewa", "nya", "ny"},
        {"Nyamwezi", "nym", nullptr},
        {"Nyankole", "nyn", nullptr},
        {"Nyoro", "nyo", nullptr},
        {"Nzima", "nzi", nullptr},
        {"Occitan (post 1500}", "oci", "oc", nullptr, MAKELCID(MAKELANGID(LANG_OCCITAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Ojibwa", "oji", "oj"},
        {"Oriya", "ori", "or"},
        {"Oromo", "orm", "om"},
        {"Osage", "osa", nullptr},
        {"Ossetian; Ossetic", "oss", "os"},
        {"Turkish, Ottoman (1500-1928)", "ota", nullptr},
        {"Otomian languages", "oto", nullptr},
        {"Papuan (Other)", "paa", nullptr},
        {"Pangasinan", "pag", nullptr},
        {"Pahlavi", "pal", nullptr},
        {"Pampanga", "pam", nullptr},
        {"Panjabi", "pan", "pa"},
        {"Papiamento", "pap", nullptr},
        {"Palauan", "pau", nullptr},
        {"Persian, Old (ca.600-400 B.C.)", "peo", nullptr},
        {"Persian", "per", "fa", "fas", MAKELCID(MAKELANGID(LANG_PERSIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Philippine (Other)", "phi", nullptr},
        {"Phoenician", "phn", nullptr},
        {"Pali", "pli", "pi"},
        {"Portuguese (Brazil)", "pob", "pb"}, // deprecated/unofficial
        {"Polish", "pol", "pl", nullptr, MAKELCID(MAKELANGID(LANG_POLISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Pohnpeian", "pon", nullptr},
        {"Portuguese", "por", "pt", nullptr, MAKELCID(MAKELANGID(LANG_PORTUGUESE, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Prakrit languages", "pra", nullptr},
        {"Provençal, Old (to 1500)", "pro", nullptr},
        {"Pushto", "pus", "ps"},
        {"Quechua", "que", "qu", nullptr, MAKELCID(MAKELANGID(LANG_QUECHUA, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Rajasthani", "raj", nullptr},
        {"Rapanui", "rap", nullptr},
        {"Rarotongan", "rar", nullptr},
        {"Romance (Other)", "roa", nullptr},
        {"Romansh", "roh", "rm"},
        {"Romany", "rom", nullptr},
        {"Romanian", "rum", "ro", "ron", MAKELCID(MAKELANGID(LANG_ROMANIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Rundi", "run", "rn"},
        {"Aromanian", "rup", nullptr},
        {"Russian", "rus", "ru", nullptr, MAKELCID(MAKELANGID(LANG_RUSSIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Sandawe", "sad", nullptr},
        {"Sango", "sag", "sg"},
        {"Yakut", "sah", nullptr, nullptr, MAKELCID(MAKELANGID(LANG_YAKUT, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"South American Indian (Other)", "sai", nullptr},
        {"Salishan languages", "sal", nullptr},
        {"Samaritan Aramaic", "sam", nullptr},
        {"Sanskrit", "san", "sa", nullptr, MAKELCID(MAKELANGID(LANG_SANSKRIT, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Sasak", "sas", nullptr},
        {"Santali", "sat", nullptr},
        {"Sicilian", "scn", nullptr},
        {"Scots", "sco", nullptr},
        {"Selkup", "sel", nullptr},
        {"Semitic (Other)", "sem", nullptr},
        {"Irish, Old (to 900)", "sga", nullptr},
        {"Sign languages", "sgn", nullptr},
        {"Shan", "shn", nullptr},
        {"Sidamo", "sid", nullptr},
        {"Sinhalese", "sin", "si", nullptr, MAKELCID(MAKELANGID(LANG_SINHALESE, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Siouan languages", "sio", nullptr},
        {"Sino-Tibetan (Other)", "sit", nullptr},
        {"Slavic (Other)", "sla", nullptr},
        {"Slovak", "slo", "sk", "slk", MAKELCID(MAKELANGID(LANG_SLOVAK, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Slovenian", "slv", "sl", nullptr, MAKELCID(MAKELANGID(LANG_SLOVENIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Southern Sami", "sma", nullptr},
        {"Northern Sami", "sme", "se"},
        {"Sami languages (Other)", "smi", nullptr},
        {"Lule Sami", "smj", nullptr},
        {"Inari Sami", "smn", nullptr},
        {"Samoan", "smo", "sm"},
        {"Skolt Sami", "sms", nullptr},
        {"Shona", "sna", "sn"},
        {"Sindhi", "snd", "sd", nullptr, MAKELCID(MAKELANGID(LANG_SINDHI, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Soninke", "snk", nullptr},
        {"Sogdian", "sog", nullptr},
        {"Somali", "som", "so"},
        {"Songhai", "son", nullptr},
        {"Sotho, Southern", "sot", "st", nullptr, MAKELCID(MAKELANGID(LANG_SOTHO, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Spanish", "spa", "es", "esp", MAKELCID(MAKELANGID(LANG_SPANISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Sardinian", "srd", "sc"},
        {"Sranan Tongo", "srn", nullptr},
        {"Serbian", "srp", "sr", "scc", MAKELCID(MAKELANGID(LANG_SERBIAN_NEUTRAL, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Serer", "srr", nullptr},
        {"Nilo-Saharan (Other)", "ssa", nullptr},
        {"Swati", "ssw", "ss"},
        {"Sukuma", "suk", nullptr},
        {"Sundanese", "sun", "su"},
        {"Susu", "sus", nullptr},
        {"Sumerian", "sux", nullptr},
        {"Swahili", "swa", "sw", nullptr, MAKELCID(MAKELANGID(LANG_SWAHILI, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Swedish", "swe", "sv", nullptr, MAKELCID(MAKELANGID(LANG_SWEDISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Classical Syriac", "syc", nullptr},
        {"Syriac", "syr", nullptr, nullptr, MAKELCID(MAKELANGID(LANG_SYRIAC, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Tahitian", "tah", "ty"},
        {"Tai (Other)", "tai", nullptr},
        {"Tamil", "tam", "ta", nullptr, MAKELCID(MAKELANGID(LANG_TAMIL, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Tatar", "tat", "tt", nullptr, MAKELCID(MAKELANGID(LANG_TATAR, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Telugu", "tel", "te", nullptr, MAKELCID(MAKELANGID(LANG_TELUGU, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Timne", "tem", nullptr},
        {"Tereno", "ter", nullptr},
        {"Tetum", "tet", nullptr},
        {"Tajik", "tgk", "tg", nullptr, MAKELCID(MAKELANGID(LANG_TAJIK, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Tagalog", "tgl", "tl"},
        {"Thai", "tha", "th", nullptr, MAKELCID(MAKELANGID(LANG_THAI, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Tibetan", "tib", "bo", "bod", MAKELCID(MAKELANGID(LANG_TIBETAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Tigre", "tig", nullptr},
        {"Tigrinya", "tir", "ti", nullptr, MAKELCID(MAKELANGID(LANG_TIGRIGNA, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Tiv", "tiv", nullptr},
        {"Tokelau", "tkl", nullptr},
        {"Klingon", "tlh", nullptr},
        {"Tlingit", "tli", nullptr},
        {"Tamashek", "tmh", nullptr},
        {"Tonga (Nyasa)", "tog", nullptr},
        {"Tonga (Tonga Islands)", "ton", "to"},
        {"Tok Pisin", "tpi", nullptr},
        {"Tsimshian", "tsi", nullptr},
        {"Tswana", "tsn", "tn", nullptr, MAKELCID(MAKELANGID(LANG_TSWANA, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Tsonga", "tso", "ts"},
        {"Turkmen", "tuk", "tk", nullptr, MAKELCID(MAKELANGID(LANG_TURKMEN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Tumbuka", "tum", nullptr},
        {"Tupi languages", "tup", nullptr},
        {"Turkish", "tur", "tr", nullptr, MAKELCID(MAKELANGID(LANG_TURKISH, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Altaic (Other)", "tut", nullptr},
        {"Tuvalu", "tvl", nullptr},
        {"Twi", "twi", "tw"},
        {"Tuvinian", "tyv", nullptr},
        {"Udmurt", "udm", nullptr},
        {"Ugaritic", "uga", nullptr},
        {"Uighur", "uig", "ug", nullptr, MAKELCID(MAKELANGID(LANG_UIGHUR, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Ukrainian", "ukr", "uk", nullptr, MAKELCID(MAKELANGID(LANG_UKRAINIAN, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Umbundu", "umb", nullptr},
        {"Undetermined", "und", nullptr},
        {"Urdu", "urd", "ur", nullptr, MAKELCID(MAKELANGID(LANG_URDU, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Uzbek", "uzb", "uz", nullptr, MAKELCID(MAKELANGID(LANG_UZBEK, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Vai", "vai", nullptr},
        {"Venda", "ven", "ve"},
        {"Vietnamese", "vie", "vi", nullptr, MAKELCID(MAKELANGID(LANG_VIETNAMESE, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Volapük", "vol", "vo"},
        {"Votic", "vot", nullptr},
        {"Wakashan languages", "wak", nullptr},
        {"Walamo", "wal", nullptr},
        {"Waray", "war", nullptr},
        {"Washo", "was", nullptr},
        {"Welsh", "wel", "cy", "cym", MAKELCID(MAKELANGID(LANG_WELSH, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Sorbian languages", "wen", nullptr},
        {"Walloon", "wln", "wa"},
        {"Wolof", "wol", "wo", nullptr, MAKELCID(MAKELANGID(LANG_WOLOF, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Kalmyk", "xal", nullptr},
        {"Xhosa", "xho", "xh", nullptr, MAKELCID(MAKELANGID(LANG_XHOSA, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Yao", "yao", nullptr},
        {"Yapese", "yap", nullptr},
        {"Yiddish", "yid", "yi"},
        {"Yoruba", "yor", "yo", nullptr, MAKELCID(MAKELANGID(LANG_YORUBA, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Yupik languages", "ypk", nullptr},
        {"Zapotec", "zap", nullptr},
        {"Blissymbols", "zbl", nullptr},
        {"Zenaga", "zen", nullptr},
        {"Standard Moroccan Tamazight", "zgh", nullptr},
        {"Zhuang; Chuang", "zha", "za"},
        {"Zande", "znd", nullptr},
        {"Zulu", "zul", "zu", nullptr, MAKELCID(MAKELANGID(LANG_ZULU, SUBLANG_DEFAULT), SORT_DEFAULT)},
        {"Zuni", "zun", nullptr},
        {"Zaza", "zza", nullptr},
        {nullptr, nullptr, nullptr},
        {"No subtitles", "---", nullptr, nullptr, (LCID)LCID_NOSUBTITLES},
};

std::string ISO6391ToLanguage(LPCSTR code)
{
    CHAR tmp[2 + 1];
    strncpy_s(tmp, code, 2);
    tmp[2] = 0;
    _strlwr_s(tmp);
    for (size_t i = 0, j = countof(s_isolangs); i < j; i++)
    {
        if (s_isolangs[i].iso6391 && !strcmp(s_isolangs[i].iso6391, tmp))
        {
            std::string ret = std::string(s_isolangs[i].name);
            size_t i = ret.find(';');
            if (i != std::string::npos)
            {
                ret = ret.substr(0, i);
            }
            return ret;
        }
    }
    return std::string();
}

std::string ISO6392ToLanguage(LPCSTR code)
{
    CHAR tmp[3 + 1];
    strncpy_s(tmp, code, 3);
    tmp[3] = 0;
    _strlwr_s(tmp);
    for (size_t i = 0, j = countof(s_isolangs); i < j; i++)
    {
        if ((s_isolangs[i].iso6392 && !strcmp(s_isolangs[i].iso6392, tmp)) ||
            (s_isolangs[i].iso6392_2 && !strcmp(s_isolangs[i].iso6392_2, tmp)))
        {
            std::string ret = std::string(s_isolangs[i].name);
            size_t i = ret.find(';');
            if (i != std::string::npos)
            {
                ret = ret.substr(0, i);
            }
            return ret;
        }
    }
    return std::string();
}

std::string ProbeLangForLanguage(LPCSTR code)
{
    if (strlen(code) == 3)
    {
        return ISO6392ToLanguage(code);
    }
    else if (strlen(code) >= 2)
    {
        return ISO6391ToLanguage(code);
    }
    return std::string();
}

static std::string ISO6392Check(LPCSTR lang)
{
    CHAR tmp[3 + 1];
    strncpy_s(tmp, lang, 3);
    tmp[3] = 0;
    _strlwr_s(tmp);
    for (size_t i = 0, j = countof(s_isolangs); i < j; i++)
    {
        if ((s_isolangs[i].iso6392 && !strcmp(s_isolangs[i].iso6392, tmp)) ||
            (s_isolangs[i].iso6392_2 && !strcmp(s_isolangs[i].iso6392_2, tmp)))
        {
            return std::string(s_isolangs[i].iso6392);
        }
    }
    return std::string(tmp);
}

static std::string LanguageToISO6392(LPCSTR code)
{
    for (size_t i = 0, j = countof(s_isolangs); i < j; i++)
    {
        if ((s_isolangs[i].name && !_stricmp(s_isolangs[i].name, code)))
        {
            return std::string(s_isolangs[i].iso6392);
        }
    }
    return std::string();
}

std::string ProbeForISO6392(LPCSTR lang)
{
    std::string isoLang;
    if (strlen(lang) == 2)
    {
        isoLang = ISO6391To6392(lang);
    }
    else if (strlen(lang) == 3)
    {
        isoLang = ISO6392Check(lang);
    }
    else if (strlen(lang) > 3)
    {
        isoLang = LanguageToISO6392(lang);
        if (isoLang.empty())
        {
            std::regex ogmRegex("\\[([[:alpha:]]{3})\\]");
            std::cmatch res;
            bool found = std::regex_search(lang, res, ogmRegex);
            if (found && !res[1].str().empty())
            {
                isoLang = ISO6392Check(res[1].str().c_str());
            }
        }
    }
    if (isoLang.empty())
        isoLang = std::string(lang);
    return isoLang;
}

LCID ISO6391ToLcid(LPCSTR code)
{
    CHAR tmp[2 + 1];
    strncpy_s(tmp, code, 2);
    tmp[2] = 0;
    _strlwr_s(tmp);
    for (size_t i = 0, j = countof(s_isolangs); i < j; i++)
    {
        if (s_isolangs[i].iso6391 && !strcmp(s_isolangs[i].iso6391, tmp))
        {
            return s_isolangs[i].lcid;
        }
    }
    return 0;
}

LCID ISO6392ToLcid(LPCSTR code)
{
    CHAR tmp[3 + 1];
    strncpy_s(tmp, code, 3);
    tmp[3] = 0;
    _strlwr_s(tmp);
    for (size_t i = 0, j = countof(s_isolangs); i < j; i++)
    {
        if ((s_isolangs[i].iso6392 && !strcmp(s_isolangs[i].iso6392, tmp)) ||
            (s_isolangs[i].iso6392_2 && !strcmp(s_isolangs[i].iso6392_2, tmp)))
        {
            return s_isolangs[i].lcid;
        }
    }
    return 0;
}

std::string ISO6391To6392(LPCSTR code)
{
    CHAR tmp[2 + 1];
    strncpy_s(tmp, code, 2);
    tmp[2] = 0;
    _strlwr_s(tmp);
    for (size_t i = 0, j = countof(s_isolangs); i < j; i++)
    {
        if (s_isolangs[i].iso6391 && !strcmp(s_isolangs[i].iso6391, tmp))
        {
            return s_isolangs[i].iso6392;
        }
    }
    return std::string(code);
}

std::string ISO6392To6391(LPCSTR code)
{
    CHAR tmp[3 + 1];
    strncpy_s(tmp, code, 3);
    tmp[3] = 0;
    _strlwr_s(tmp);
    for (size_t i = 0, j = countof(s_isolangs); i < j; i++)
    {
        if ((s_isolangs[i].iso6392 && !strcmp(s_isolangs[i].iso6392, tmp)) ||
            (s_isolangs[i].iso6392_2 && !strcmp(s_isolangs[i].iso6392_2, tmp)))
        {
            return s_isolangs[i].iso6391;
        }
    }
    return std::string();
}

LCID ProbeLangForLCID(LPCSTR code)
{
    if (strlen(code) == 3)
    {
        return ISO6392ToLcid(code);
    }
    else if (strlen(code) >= 2)
    {
        return ISO6391ToLcid(code);
    }
    return 0;
}
