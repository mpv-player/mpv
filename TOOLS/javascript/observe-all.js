// Test script for property change notification mechanism.

var properties = mp.get_property_native("property-list");
for (var i in properties) {
    mp.observe_property(properties[i], "native", function observer(name, val) {
        print("property '" + name + "' changed to " + JSON.stringify(val));
    });
}
