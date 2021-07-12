function subtimes() {
  mp.msg.info("sub-start: " + mp.get_property_number("sub-start"));
  mp.msg.info("sub-end: " + mp.get_property_number("sub-end"));
  mp.msg.info("sub-text: " + mp.get_property_native("sub-text"));
}

mp.add_key_binding("t", "subtimes", subtimes);

function secondary_subtimes() {
  mp.msg.info("secondary-sub-start: " + mp.get_property_number("secondary-sub-start"));
  mp.msg.info("secondary-sub-end: " + mp.get_property_number("secondary-sub-end"));
  mp.msg.info("secondary-sub-text: " + mp.get_property_native("secondary-sub-text"));
}

mp.add_key_binding("T", "secondary_subtimes", secondary_subtimes);