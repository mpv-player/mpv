function subtimes() {
  mp.msg.info("sub-start: " + mp.get_property_number("sub-start"));
  mp.msg.info("sub-end: " + mp.get_property_number("sub-end"));
  mp.msg.info("sub-text: " + mp.get_property_native("sub-text"));
}

mp.add_key_binding("t", "subtimes", subtimes);
