add_executable(pico_tnc
        main.c
        tnc.c
        send.c
        receive.c
        decode.c
        filter.c
        bell202.c
        ax25.c
        test.c
        packet_table.c
        usb_input.c
        usb_output.c
        cmd.c
        serial.c
        tty.c
        flash.c
        gps.c
        unproto.c
        digipeat.c
        beacon.c
        kiss.c
        )

target_include_directories(pico_tnc PRIVATE
        include
)

target_link_libraries(pico_tnc
		pico_stdlib
        #pico_stdio_uart
        #pico_stdio_usb
		hardware_adc
		hardware_dma
        hardware_pwm
        hardware_uart
		# For the dummy output:
		#hardware_pio
		#pico_multicore
		)

pico_enable_stdio_usb(pico_tnc 1)
pico_enable_stdio_uart(pico_tnc 1)

# create map/bin/hex file etc.
pico_add_extra_outputs(pico_tnc)

# add url via pico_set_program_url
example_auto_set_url(pico_tnc)
