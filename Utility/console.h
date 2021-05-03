/* Copyright (C) 20xx Jason Tully - All Rights Reserved
 * You may use, distribute and modify this code under the
 * terms of the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License
 * http://www.supersinfulsilicon.com/
 *
This work is licensed under the Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License.
To view a copy of this license, visit http://creativecommons.org/licenses/by-nc-sa/4.0/
or send a letter to Creative Commons, PO Box 1866, Mountain View, CA 94042, USA.
 */

#pragma once 
#include <stdint.h>
#include <conio.h>
#include <ConsoleApi2.h>

// main data structure returned from initConsole<columns,rows>()

template<uint32_t const columns, uint32_t const rows>
struct Console
{
	constexpr uint32_t const width() const { return(columns); }
	constexpr uint32_t const height() const { return(rows); }

	HANDLE const
		hDoubleBuffer[2];

	bool dblbuffer_index;

	CHAR_INFO
		backbuffer[rows * columns];

	void init() {

		dblbuffer_index = false;

		COORD const dwBufferSize = { columns, rows };
		COORD const dwBufferCoord = { 0, 0 };
		SMALL_RECT rcRegion = { 0, 0, columns - 1, rows - 1 };

		ReadConsoleOutput(hDoubleBuffer[(uint32_t const)!dblbuffer_index], (CHAR_INFO*)backbuffer, dwBufferSize,
			dwBufferCoord, &rcRegion);

		SetConsoleActiveScreenBuffer(hDoubleBuffer[(uint32_t const)dblbuffer_index]);
	}
	void blit() {

		uint32_t const backbuffer_index((uint32_t const)!dblbuffer_index);
		uint32_t const frontbuffer_index((uint32_t const)dblbuffer_index);

		COORD const coordCursor = { 0, 0 };    /* here's where we'll home the
									cursor */
		COORD const dwBufferSize = { columns, rows };
		COORD const dwBufferCoord = { 0, 0 };
		SMALL_RECT rcRegion = { 0, 0, columns - 1, rows - 1 };

		ReadConsoleOutput(hDoubleBuffer[backbuffer_index], (CHAR_INFO*)backbuffer, dwBufferSize,
			dwBufferCoord, &rcRegion);

		SetConsoleCursorPosition(hDoubleBuffer[frontbuffer_index], coordCursor);
		WriteConsoleOutput(hDoubleBuffer[frontbuffer_index], (CHAR_INFO*)backbuffer, dwBufferSize,
			dwBufferCoord, &rcRegion);

		SetConsoleCursorPosition(hDoubleBuffer[backbuffer_index], coordCursor);
	}
	
	void set_cursor(uint32_t const x, uint32_t const y) {

		uint32_t const backbuffer_index((uint32_t const)!dblbuffer_index);

		COORD const coordCursor = { (SHORT)x, (SHORT)y };    /* here's where we'll home the
									cursor */
		SetConsoleCursorPosition(hDoubleBuffer[backbuffer_index], coordCursor);
	}
	auto const get_cursor() {

		uint32_t const backbuffer_index((uint32_t const)!dblbuffer_index);

		CONSOLE_SCREEN_BUFFER_INFO cbsi;
		GetConsoleScreenBufferInfo(hDoubleBuffer[backbuffer_index], &cbsi);
		
		struct _ {
			uint32_t const x, y;
		};

		return(_{ (uint32_t)cbsi.dwCursorPosition.X, (uint32_t)cbsi.dwCursorPosition.Y });
	}
	void clear_current_line() {

		uint32_t const backbuffer_index((uint32_t const)!dblbuffer_index);

		auto const [x, y] = get_cursor();    
		
		DWORD cCharsWritten;
		COORD coordScreen = { 0, (SHORT)y };

		FillConsoleOutputCharacter(hDoubleBuffer[backbuffer_index], (TCHAR)' ',
			columns, coordScreen, &cCharsWritten);

		// restore cursor
		set_cursor(x, y);
	}

	// "press any key to continue"
	void wait_for_input() {
		FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
		(void)_getch();
	}
};

// method to clear screen (slow) good for initial screen output
static inline void cls(HANDLE hConsole)
{
	COORD coordScreen = { 0, 0 };    /* here's where we'll home the
										cursor */
	DWORD cCharsWritten;
	CONSOLE_SCREEN_BUFFER_INFO csbi; /* to get buffer info */
	DWORD dwConSize;                 /* number of character cells in
										the current buffer */

										/* get the number of character cells in the current buffer */

	GetConsoleScreenBufferInfo(hConsole, &csbi);
	dwConSize = csbi.dwSize.X * csbi.dwSize.Y;

	CONSOLE_CURSOR_INFO curinfo;
	curinfo.dwSize = 100;
	curinfo.bVisible = FALSE;
	SetConsoleCursorInfo(hConsole, &curinfo);

	/* fill the entire screen with blanks */

	FillConsoleOutputCharacter(hConsole, (TCHAR)' ',
		dwConSize, coordScreen, &cCharsWritten);

	/* get the current text attribute */

	GetConsoleScreenBufferInfo(hConsole, &csbi);

	/* now set the buffer's attributes accordingly */

	FillConsoleOutputAttribute(hConsole, csbi.wAttributes,
		dwConSize, coordScreen, &cCharsWritten);

	/* put the cursor at (0, 0) */

	SetConsoleCursorPosition(hConsole, coordScreen);

}

template<uint32_t const columns, uint32_t const rows>
static inline auto& initConsole()
{
	BOOL success(FALSE);
	HANDLE const hConsole(GetStdHandle(STD_OUTPUT_HANDLE));

	COORD const coordScreen = { columns, rows };
	success = SetConsoleScreenBufferSize(hConsole, coordScreen);

	SMALL_RECT const window_rect{ 0, 0, columns - 1, rows - 1 };
	success = SetConsoleWindowInfo(hConsole, TRUE, &window_rect);

	cls(hConsole);

	COORD const coordCursor = { 0, 0 };    /* here's where we'll home the
									cursor */
	SetConsoleCursorPosition(hConsole, coordScreen);

	//HANDLE const hProcess(GetCurrentProcess());
	HANDLE hNewScreenBuffer(hConsole);

	//DuplicateHandle(hProcess, hConsole, hProcess, &hNewScreenBuffer, 0, FALSE, DUPLICATE_SAME_ACCESS);

	/*
	HANDLE const hNewScreenBuffer = CreateConsoleScreenBuffer(
		GENERIC_READ |           // read/write access
		GENERIC_WRITE,
		FILE_SHARE_READ |
		FILE_SHARE_WRITE,        // shared
		NULL,                    // default security attributes
		CONSOLE_TEXTMODE_BUFFER, // must be TEXTMODE
		NULL);
	*/
	success = SetConsoleActiveScreenBuffer(hNewScreenBuffer);

	cls(hNewScreenBuffer);
	SetConsoleCursorPosition(hNewScreenBuffer, coordScreen);

	////
	static Console<columns, rows> console{ {hNewScreenBuffer, hConsole} };
	////

	// must initialize backbuffer
	console.init();

	return(console);
}