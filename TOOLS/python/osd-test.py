from mpvclient import mpv  # type: ignore

things = []
for _ in range(2):
    things.append({
        "osd1": mpv.create_osd_overlay("ass-events"),
        "osd2": mpv.create_osd_overlay("ass-events"),
    })

things[0]["text"] = "{\\an5}hello\\Nworld"
things[1]["text"] = "{\\pos(400, 200)}something something"


def the_do_hickky_thing():
    for i, thing in enumerate(things):
        thing["osd1"].data = thing["text"]
        thing["osd1"].compute_bounds = True
        # thing.osd1.hidden = true
        res = thing["osd1"].update()

        thing["osd2"].hidden = True
        if res is not None and res["x0"] is not None:
            draw = mpv.ass_new()
            draw.append("{\\alpha&H80}")
            draw.draw_start()
            draw.pos(0, 0)
            draw.rect_cw(res["x0"], res["y0"], res["x1"], res["y1"])
            draw.draw_stop()
            thing["osd2"].hidden = False
            thing["osd2"].data = draw.text

        thing["osd2"].update()


mpv.add_periodic_timer(2, the_do_hickky_thing, name="the_do_hickky_thing")
