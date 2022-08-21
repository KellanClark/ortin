
#include "menus/debug.hpp"
#include "imgui_memory_editor.h"

#include <fstream>

DebugMenu::DebugMenu() {
	showLogs = false;
	showArm9Debug = false;
	showArm7Debug = false;
	showMemEditor9 = false;
	showMemEditor7 = false;
	showIoReg9 = false;
	showIoReg7 = false;

	arm9disasm.defaultSettings();
	arm7disasm.defaultSettings();
}

DebugMenu::~DebugMenu() {
	//
}

void DebugMenu::drawMenu() {
	if (ImGui::BeginMenu("Debug")) {
		ImGui::MenuItem("Logs", nullptr, &showLogs);
		ImGui::MenuItem("ARM9 Debug", nullptr, &showArm9Debug);
		ImGui::MenuItem("ARM7 Debug", nullptr, &showArm7Debug);
		ImGui::MenuItem("ARM9 Memory", nullptr, &showMemEditor9);
		ImGui::MenuItem("ARM7 Memory", nullptr, &showMemEditor7);
		ImGui::MenuItem("ARM9 IO", nullptr, &showIoReg9);
		ImGui::MenuItem("ARM7 IO", nullptr, &showIoReg7);

		ImGui::EndMenu();
	}
}

void DebugMenu::drawWindows() {
	if (showLogs) logsWindow();
	if (showArm9Debug) arm9DebugWindow();
	if (showArm7Debug) arm7DebugWindow();
	if (showMemEditor9) memEditor9Window();
	if (showMemEditor7) memEditor7Window();
	if (showIoReg9) ioReg9Window();
	if (showIoReg7) ioReg7Window();
}

// A helper function for input boxes
u32 numberInput(const char *text, bool hex, u32 currentValue, u32 max) {
	char buf[128];
	sprintf(buf, hex ? "0x%X" : "%d", currentValue);
	ImGui::InputText(text, buf, 128, hex ? ImGuiInputTextFlags_CharsHexadecimal : ImGuiInputTextFlags_CharsDecimal);
	return (u32)std::clamp(strtoul(buf, NULL, 0), (unsigned long)0, (unsigned long)max);
}

struct MemoryRegion {
	std::string name;
	u8 *pointer;
	int size;
};

void DebugMenu::logsWindow() {
	static bool shouldAutoscroll = true;

	ImGui::SetNextWindowSize(ImVec2(700, 600), ImGuiCond_FirstUseEver);
	ImGui::Begin("Logs", &showLogs);

	ImGui::Checkbox("Auto-scroll", &shouldAutoscroll);
	ImGui::SameLine();
	if (ImGui::Button("Clear Logs")) {
		ortin.nds.addThreadEvent(NDS::CLEAR_LOG);
	}
	ImGui::SameLine();
	if (ImGui::Button("Save Logs")) {
		std::ofstream systemLogFileStream{"log", std::ios::trunc};
		systemLogFileStream << ortin.nds.log.str();
		systemLogFileStream.close();
	}

	if (ImGui::TreeNode("ARM9 Disassembler Options")) {
		ImGui::Checkbox("Show AL Condition", (bool *)&arm9disasm.options.showALCondition);
		ImGui::Checkbox("Always Show S Bit", (bool *)&arm9disasm.options.alwaysShowSBit);
		ImGui::Checkbox("Show Operands in Hex", (bool *)&arm9disasm.options.printOperandsHex);
		ImGui::Checkbox("Show Addresses in Hex", (bool *)&arm9disasm.options.printAddressesHex);
		ImGui::Checkbox("Simplify Register Names", (bool *)&arm9disasm.options.simplifyRegisterNames);
		ImGui::Checkbox("Simplify LDM and STM to PUSH and POP", (bool *)&arm9disasm.options.simplifyPushPop);
		ImGui::Checkbox("Use Alternative Stack Suffixes for LDM and STM", (bool *)&arm9disasm.options.ldmStmStackSuffixes);
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("ARM7 Disassembler Options")) {
		ImGui::Checkbox("Show AL Condition", (bool *)&arm7disasm.options.showALCondition);
		ImGui::Checkbox("Always Show S Bit", (bool *)&arm7disasm.options.alwaysShowSBit);
		ImGui::Checkbox("Show Operands in Hex", (bool *)&arm7disasm.options.printOperandsHex);
		ImGui::Checkbox("Show Addresses in Hex", (bool *)&arm7disasm.options.printAddressesHex);
		ImGui::Checkbox("Simplify Register Names", (bool *)&arm7disasm.options.simplifyRegisterNames);
		ImGui::Checkbox("Simplify LDM and STM to PUSH and POP", (bool *)&arm7disasm.options.simplifyPushPop);
		ImGui::Checkbox("Use Alternative Stack Suffixes for LDM and STM", (bool *)&arm7disasm.options.ldmStmStackSuffixes);
		ImGui::TreePop();
	}

	ImGui::Spacing();
	ImGui::Separator();

	if (ImGui::TreeNode("Log")) {
		ImGui::BeginChild("logS", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
		ImGui::TextUnformatted(ortin.nds.log.str().c_str());
		if (shouldAutoscroll)
			ImGui::SetScrollHereY(1.0f);
		ImGui::EndChild();
		ImGui::TreePop();
	}

	ImGui::End();
}

void DebugMenu::arm9DebugWindow() {
	auto& cpu = ortin.nds.nds9.cpu;

	ImGui::SetNextWindowSize(ImVec2(760, 480));
	ImGui::Begin("ARM9 Debug", &showArm9Debug);

	if (ImGui::Button("Reset"))
		ortin.nds.addThreadEvent(NDS::RESET);
	ImGui::SameLine();
	if (ortin.nds.running) {
		if (ImGui::Button("Pause"))
			ortin.nds.addThreadEvent(NDS::STOP);
	} else {
		if (ImGui::Button("Unpause"))
			ortin.nds.addThreadEvent(NDS::START);
	}

	ImGui::Spacing();
	if (ImGui::Button("Step")) {
		ortin.nds.addThreadEvent(NDS::STEP_ARM9);
	}
	ImGui::Separator();

	// CPU Status
	{
		ImGui::BeginChild("ChildL#arm9", ImVec2(525, 0), false);

		std::string tmp = arm9disasm.disassemble(cpu.reg.R[15] - (cpu.reg.thumbMode ? 4 : 8), cpu.pipelineOpcode3, cpu.reg.thumbMode);
		ImGui::Text("Current Opcode:  %s", tmp.c_str());
		ImGui::Spacing();
		if (ImGui::BeginTable("registers9", 8, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV)) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("Current");
			ImGui::TableNextColumn();
			ImGui::Text("User/\nSystem");
			ImGui::TableNextColumn();
			ImGui::Text("FIQ");
			ImGui::TableNextColumn();
			ImGui::Text("IRQ");
			ImGui::TableNextColumn();
			ImGui::Text("Supervisor");
			ImGui::TableNextColumn();
			ImGui::Text("Abort");
			ImGui::TableNextColumn();
			ImGui::Text("Undefined");
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r0:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[0]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r1:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[1]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r2:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[2]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r3:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[3]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r4:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[4]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r5:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[5]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r6:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[6]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r7:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[7]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r8:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R8_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R8_fiq);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r9:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[9]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R9_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R9_fiq);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r10:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[10]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R10_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R10_fiq);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r11:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[11]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R11_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R11_fiq);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r12:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[12]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R12_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R12_fiq);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r13:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[13]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R13_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R13_fiq);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R13_irq);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R13_svc);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R13_abt);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R13_und);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r14:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[14]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R14_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R14_fiq);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R14_irq);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R14_svc);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R14_abt);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R14_und);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r15:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[15]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("SPSR:");
			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%08X", cpu.reg.SPSR_fiq);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.SPSR_irq);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.SPSR_svc);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.SPSR_abt);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.SPSR_und);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("CPSR:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.CPSR);
			ImGui::TableNextRow();

			ImGui::EndTable();
		}
		ImGui::Spacing();
		bool n = cpu.reg.flagN;
		ImGui::Checkbox("N", &n);
		ImGui::SameLine();
		bool z = cpu.reg.flagZ;
		ImGui::Checkbox("Z", &z);
		ImGui::SameLine();
		bool c = cpu.reg.flagC;
		ImGui::Checkbox("C", &c);
		ImGui::SameLine();
		bool v = cpu.reg.flagV;
		ImGui::Checkbox("V", &v);
		ImGui::SameLine();
		bool q = cpu.reg.flagQ;
		ImGui::Checkbox("Q", &q);
		ImGui::SameLine();
		bool i = cpu.reg.irqDisable;
		ImGui::Checkbox("I", &i);
		ImGui::SameLine();
		bool f = cpu.reg.fiqDisable;
		ImGui::Checkbox("F", &f);
		ImGui::SameLine();
		bool t = cpu.reg.thumbMode;
		ImGui::Checkbox("T", &t);

		ImGui::EndChild();
	}

	ImGui::SameLine();

	// Breakpoints Window
	{
		static u32 breakpointAddress = 0;
		static int selectedBreakpoint = -1;
		static std::vector<u32> breakpoints {};
		ImGui::BeginChild("ChildR##bkpt9", ImVec2(210, ImGui::GetContentRegionAvail().y), true);

		ImGui::Text("Breakpoints");
		ImGui::Separator();

		ImGui::PushItemWidth(80);
		breakpointAddress = numberInput("##bkptin9", true, breakpointAddress, 0x0FFFFFFF);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("Add Breakpoint")) {
			bool match = false;
			for (u32 i : breakpoints) {
				if (i == breakpointAddress)
					match = true;
			}

			if (!match) {
				cpu.addBreakpoint(breakpointAddress);
				breakpoints.push_back(breakpointAddress);
			}
		}

		if (ImGui::Button("Delete Selected")) {
			if (selectedBreakpoint != -1) {
				cpu.removeBreakpoint(breakpoints[selectedBreakpoint]);
				breakpoints.erase(breakpoints.begin() + selectedBreakpoint);
				selectedBreakpoint = -1;
			}
		}

		for (int i = 0; i < breakpoints.size(); i++) {
			if (ImGui::Selectable(fmt::format("0x{:0>7X}", breakpoints[i]).c_str(), selectedBreakpoint == i))
				selectedBreakpoint = i;
		}

		ImGui::EndChild();
	}

	ImGui::End();
}

void DebugMenu::arm7DebugWindow() {
	auto& cpu = ortin.nds.nds7.cpu;

	ImGui::SetNextWindowSize(ImVec2(760, 480));
	ImGui::Begin("ARM7 Debug", &showArm7Debug);

	if (ImGui::Button("Reset"))
		ortin.nds.addThreadEvent(NDS::RESET);
	ImGui::SameLine();
	if (ortin.nds.running) {
		if (ImGui::Button("Pause"))
			ortin.nds.addThreadEvent(NDS::STOP);
	} else {
		if (ImGui::Button("Unpause"))
			ortin.nds.addThreadEvent(NDS::START);
	}

	ImGui::Spacing();
	if (ImGui::Button("Step")) {
		ortin.nds.addThreadEvent(NDS::STEP_ARM7);
	}
	ImGui::Separator();

	// CPU Status
	{
		ImGui::BeginChild("ChildL#arm7", ImVec2(525, 0), false);

		std::string tmp = arm9disasm.disassemble(cpu.reg.R[15] - (cpu.reg.thumbMode ? 4 : 8), cpu.pipelineOpcode3, cpu.reg.thumbMode);
		ImGui::Text("Current Opcode:  %s", tmp.c_str());
		ImGui::Spacing();
		if (ImGui::BeginTable("registers7", 8, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_BordersInnerV)) {
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("Current");
			ImGui::TableNextColumn();
			ImGui::Text("User/\nSystem");
			ImGui::TableNextColumn();
			ImGui::Text("FIQ");
			ImGui::TableNextColumn();
			ImGui::Text("IRQ");
			ImGui::TableNextColumn();
			ImGui::Text("Supervisor");
			ImGui::TableNextColumn();
			ImGui::Text("Abort");
			ImGui::TableNextColumn();
			ImGui::Text("Undefined");
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r0:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[0]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r1:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[1]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r2:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[2]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r3:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[3]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r4:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[4]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r5:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[5]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r6:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[6]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r7:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[7]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r8:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[8]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R8_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R8_fiq);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r9:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[9]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R9_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R9_fiq);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r10:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[10]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R10_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R10_fiq);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r11:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[11]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R11_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R11_fiq);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r12:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[12]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R12_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R12_fiq);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r13:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[13]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R13_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R13_fiq);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R13_irq);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R13_svc);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R13_abt);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R13_und);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r14:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[14]);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R14_user);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R14_fiq);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R14_irq);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R14_svc);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R14_abt);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R14_und);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("r15:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.R[15]);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("SPSR:");
			ImGui::TableSetColumnIndex(3);
			ImGui::Text("%08X", cpu.reg.SPSR_fiq);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.SPSR_irq);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.SPSR_svc);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.SPSR_abt);
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.SPSR_und);
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::Text("CPSR:");
			ImGui::TableNextColumn();
			ImGui::Text("%08X", cpu.reg.CPSR);
			ImGui::TableNextRow();

			ImGui::EndTable();
		}
		ImGui::Spacing();
		bool n = cpu.reg.flagN;
		ImGui::Checkbox("N", &n);
		ImGui::SameLine();
		bool z = cpu.reg.flagZ;
		ImGui::Checkbox("Z", &z);
		ImGui::SameLine();
		bool c = cpu.reg.flagC;
		ImGui::Checkbox("C", &c);
		ImGui::SameLine();
		bool v = cpu.reg.flagV;
		ImGui::Checkbox("V", &v);
		ImGui::SameLine();
		bool i = cpu.reg.irqDisable;
		ImGui::Checkbox("I", &i);
		ImGui::SameLine();
		bool f = cpu.reg.fiqDisable;
		ImGui::Checkbox("F", &f);
		ImGui::SameLine();
		bool t = cpu.reg.thumbMode;
		ImGui::Checkbox("T", &t);

		ImGui::EndChild();
	}

	ImGui::SameLine();

	// Breakpoints Window
	{
		static u32 breakpointAddress = 0;
		static int selectedBreakpoint = -1;
		static std::vector<u32> breakpoints {};
		ImGui::BeginChild("ChildR##bkpt7", ImVec2(210, ImGui::GetContentRegionAvail().y), true);

		ImGui::Text("Breakpoints");
		ImGui::Separator();

		ImGui::PushItemWidth(80);
		breakpointAddress = numberInput("##bkptin7", true, breakpointAddress, 0x0FFFFFFF);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		if (ImGui::Button("Add Breakpoint")) {
			bool match = false;
			for (u32 i : breakpoints) {
				if (i == breakpointAddress)
					match = true;
			}

			if (!match) {
				cpu.addBreakpoint(breakpointAddress);
				breakpoints.push_back(breakpointAddress);
			}
		}

		if (ImGui::Button("Delete Selected")) {
			if (selectedBreakpoint != -1) {
				cpu.removeBreakpoint(breakpoints[selectedBreakpoint]);
				breakpoints.erase(breakpoints.begin() + selectedBreakpoint);
				selectedBreakpoint = -1;
			}
		}

		for (int i = 0; i < breakpoints.size(); i++) {
			if (ImGui::Selectable(fmt::format("0x{:0>7X}", breakpoints[i]).c_str(), selectedBreakpoint == i))
				selectedBreakpoint = i;
		}

		ImGui::EndChild();
	}

	ImGui::End();
}

void DebugMenu::memEditor9Window() {
	static MemoryEditor memEditor;
	static int selectedRegion = 0;
	std::array<MemoryRegion, 1> regions = {{
		{"PSRAM/Main Memory (4MB mirrored 0x2000000 - 0x3000000)", ortin.nds.shared.psram, 0x400000}
	}};

	ImGui::SetNextWindowSize(ImVec2(570, 400), ImGuiCond_FirstUseEver);
	ImGui::Begin("Memory Editor ARM9", &showMemEditor9);

	if (ImGui::BeginCombo("Location", regions[selectedRegion].name.c_str())) {
		for (int i = 0; i < regions.size(); i++) {
			if (ImGui::MenuItem(regions[selectedRegion].name.c_str()))
				selectedRegion = i;
		}

		ImGui::EndCombo();
	}

	memEditor.DrawContents(regions[selectedRegion].pointer, regions[selectedRegion].size);

	ImGui::End();
}

void DebugMenu::memEditor7Window() {
	static MemoryEditor memEditor;
	static int selectedRegion = 0;
	std::array<MemoryRegion, 3> regions = {{
		{"PSRAM/Main Memory (4MB mirrored 0x2000000 - 0x3000000)", ortin.nds.shared.psram, 0x400000},
		{"Shared WRAM (32KB mirrored 0x3000000 - 0x3800000)", ortin.nds.shared.wram, 0x8000},
		{"ARM7 WRAM (64KB mirrored 0x3800000 - 0x4000000)", ortin.nds.nds7.wram, 0x10000}
	}};

	ImGui::SetNextWindowSize(ImVec2(570, 400), ImGuiCond_FirstUseEver);
	ImGui::Begin("Memory Editor ARM7", &showMemEditor7);

	if (ImGui::BeginCombo("Location", regions[selectedRegion].name.c_str())) {
		for (int i = 0; i < regions.size(); i++) {
			if (ImGui::MenuItem(regions[i].name.c_str()))
				selectedRegion = i;
		}

		ImGui::EndCombo();
	}

	memEditor.DrawContents(regions[selectedRegion].pointer, regions[selectedRegion].size);

	ImGui::End();
}

enum InputType {
	TEXT_BOX,
	TEXT_BOX_HEX,
	CHECKBOX,
	SPECIAL
};

struct IoField {
	std::string_view name;
	int startBit;
	int length;
	InputType type;
};

struct IoRegister {
	std::string_view name;
	std::string_view description;
	u32 address;
	int size;
	bool readable;
	bool writable;
	int numFields;
	IoField *fields;
};

static const std::array<IoRegister, 22> registers9 = {{
	{"DISPSTAT", "Display Status and Interrupt Control", 0x4000004, 2, true, true, 7, (IoField[]){
		{"V-Blank", 0, 1, CHECKBOX},
		{"H-Blank", 1, 1, CHECKBOX},
		{"VCOUNT Compare Status", 2, 1, CHECKBOX},
		{"V-Blank IRQ", 3, 1, CHECKBOX},
		{"H-Blank IRQ", 4, 1, CHECKBOX},
		{"VCOUNT Compare IRQ", 5, 1, CHECKBOX},
		{"VCOUNT Compare Value", 7, 9, SPECIAL}}},
	{"VCOUNT", "Shows the Current Scanline", 0x4000006, 2, true, false, 1, (IoField[]){
		{"Current Scanline", 0, 9}}},
	{"KEYINPUT", "Key Status (Inverted)", 0x4000130, 2, true, false, 10, (IoField[]){
		{"A", 0, 1, CHECKBOX},
		{"B", 1, 1, CHECKBOX},
		{"Select", 2, 1, CHECKBOX},
		{"Start", 3, 1, CHECKBOX},
		{"Right", 4, 1, CHECKBOX},
		{"Left", 5, 1, CHECKBOX},
		{"Up", 6, 1, CHECKBOX},
		{"Down", 7, 1, CHECKBOX},
		{"R", 8, 1, CHECKBOX},
		{"L", 9, 1, CHECKBOX}}},
	{"KEYCNT", "Key Interrupt Control", 0x4000132, 2, true, true, 12, (IoField[]){
		{"A", 0, 1, CHECKBOX},
		{"B", 1, 1, CHECKBOX},
		{"Select", 2, 1, CHECKBOX},
		{"Start", 3, 1, CHECKBOX},
		{"Right", 4, 1, CHECKBOX},
		{"Left", 5, 1, CHECKBOX},
		{"Up", 6, 1, CHECKBOX},
		{"Down", 7, 1, CHECKBOX},
		{"R", 8, 1, CHECKBOX},
		{"L", 9, 1, CHECKBOX},
		{"IRQ Enable", 14, 1, CHECKBOX},
		{"Condition", 15, 1, SPECIAL}}},
	{"IPCSYNC", "IPC Synchronize Register", 0x4000180, 2, true, true, 4, (IoField[]){
		{"Data input from IPCSYNC of remote CPU", 0, 4, TEXT_BOX_HEX},
		{"Data output to IPCSYNC of remote CPU", 8, 4, TEXT_BOX_HEX},
		{"Send IRQ to remote CPU", 13, 1, CHECKBOX},
		{"Enable IRQ from remote CPU", 14, 1, CHECKBOX}}},
	{"IPCFIFOCNT", "IPC Fifo Control Register", 0x4000184, 2, true, true, 9, (IoField[]){
		{"Send Fifo Empty Status", 0, 1, CHECKBOX},
		{"Send Fifo Full Status", 1, 1, CHECKBOX},
		{"Send Fifo Empty IRQ", 2, 1, CHECKBOX},
		{"Send Fifo Clear", 3, 1, CHECKBOX},
		{"Receive Fifo Empty Status", 8, 1, CHECKBOX},
		{"Receive Fifo Full Status", 9, 1, CHECKBOX},
		{"Receive Fifo Not Empty IRQ", 10, 1, CHECKBOX},
		{"Error, Read Empty/Send Full", 14, 1, CHECKBOX},
		{"Enable Send/Receive Fifo", 15, 1, CHECKBOX}}},
	{"IME",	"Interrupt Master Enable", 0x4000208, 4, true, true, 1, (IoField[]){
		{"Enable Interrupts", 0, 1, CHECKBOX}}},
	{"IE", "Interrupt Enable", 0x4000210, 4, true, true, 19, (IoField[]){
		{"LCD V-BLank", 0, 1, CHECKBOX},
		{"LCD H-BLank", 1, 1, CHECKBOX},
		{"LCD V-Counter Match", 2, 1, CHECKBOX},
		{"Timer 0 Overflow", 3, 1, CHECKBOX},
		{"Timer 1 Overflow", 4, 1, CHECKBOX},
		{"Timer 2 Overflow", 5, 1, CHECKBOX},
		{"Timer 3 Overflow", 6, 1, CHECKBOX},
		{"DMA 0", 8, 1, CHECKBOX},
		{"DMA 1", 9, 1, CHECKBOX},
		{"DMA 2", 10, 1, CHECKBOX},
		{"DMA 3", 11, 1, CHECKBOX},
		{"Keypad", 12, 1, CHECKBOX},
		{"GBA Slot", 13, 1, CHECKBOX},
		{"IPC Sync", 16, 1, CHECKBOX},
		{"IPC Send FIFO Empty", 17, 1, CHECKBOX},
		{"IPC Recv FIFO Not Empty", 18, 1, CHECKBOX},
		{"NDS-Slot Game Card Data Transfer Completion", 19, 1, CHECKBOX},
		{"NDS-Slot Game Card IREQ_MC", 20, 1, CHECKBOX},
		{"Geometry Command FIFO", 21, 1, CHECKBOX}}},
	{"IF", "Interrupt Request Flags", 0x4000214, 4, true, true, 19, (IoField[]){
		{"LCD V-BLank", 0, 1, CHECKBOX},
		{"LCD H-BLank", 1, 1, CHECKBOX},
		{"LCD V-Counter Match", 2, 1, CHECKBOX},
		{"Timer 0 Overflow", 3, 1, CHECKBOX},
		{"Timer 1 Overflow", 4, 1, CHECKBOX},
		{"Timer 2 Overflow", 5, 1, CHECKBOX},
		{"Timer 3 Overflow", 6, 1, CHECKBOX},
		{"DMA 0", 8, 1, CHECKBOX},
		{"DMA 1", 9, 1, CHECKBOX},
		{"DMA 2", 10, 1, CHECKBOX},
		{"DMA 3", 11, 1, CHECKBOX},
		{"Keypad", 12, 1, CHECKBOX},
		{"GBA Slot", 13, 1, CHECKBOX},
		{"IPC Sync", 16, 1, CHECKBOX},
		{"IPC Send FIFO Empty", 17, 1, CHECKBOX},
		{"IPC Recv FIFO Not Empty", 18, 1, CHECKBOX},
		{"NDS-Slot Game Card Data Transfer Completion", 19, 1, CHECKBOX},
		{"NDS-Slot Game Card IREQ_MC", 20, 1, CHECKBOX},
		{"Geometry Command FIFO", 21, 1, CHECKBOX}}},
	{"VRAMCNT_A", "VRAM Bank A Control", 0x4000240, 1, false, true, 3, (IoField[]){
		{"MST", 0, 2, TEXT_BOX},
		{"Offset", 3, 2, TEXT_BOX},
		{"Enable", 7, 1, CHECKBOX}}},
	{"VRAMCNT_B", "VRAM Bank B Control", 0x4000241, 1, false, true, 3, (IoField[]){
		{"MST", 0, 2, TEXT_BOX},
		{"Offset", 3, 2, TEXT_BOX},
		{"Enable", 7, 1, CHECKBOX}}},
	{"VRAMCNT_C", "VRAM Bank C Control", 0x4000242, 1, false, true, 3, (IoField[]){
		{"MST", 0, 3, TEXT_BOX},
		{"Offset", 3, 2, TEXT_BOX},
		{"Enable", 7, 1, CHECKBOX}}},
	{"VRAMCNT_D", "VRAM Bank D Control", 0x4000243, 1, false, true, 3, (IoField[]){
		{"MST", 0, 3, TEXT_BOX},
		{"Offset", 3, 2, TEXT_BOX},
		{"Enable", 7, 1, CHECKBOX}}},
	{"VRAMCNT_E", "VRAM Bank E Control", 0x4000244, 1, false, true, 2, (IoField[]){
		{"MST", 0, 3, TEXT_BOX},
		{"Enable", 7, 1, CHECKBOX}}},
	{"VRAMCNT_F", "VRAM Bank F Control", 0x4000245, 1, false, true, 3, (IoField[]){
		{"MST", 0, 3, TEXT_BOX},
		{"Offset", 3, 2, TEXT_BOX},
		{"Enable", 7, 1, CHECKBOX}}},
	{"VRAMCNT_G", "VRAM Bank G Control", 0x4000246, 1, false, true, 3, (IoField[]){
		{"MST", 0, 3, TEXT_BOX},
		{"Offset", 3, 2, TEXT_BOX},
		{"Enable", 7, 1, CHECKBOX}}},
	{"WRAMCNT",	"WRAM Bank Control", 0x4000247, 1, true, true, 1, (IoField[]){
		{"Mapping", 0, 2, SPECIAL}}},
	{"VRAMCNT_H", "VRAM Bank D Control", 0x4000248, 1, false, true, 2, (IoField[]){
		{"MST", 0, 2, TEXT_BOX},
		{"Enable", 7, 1, CHECKBOX}}},
	{"VRAMCNT_I", "VRAM Bank D Control", 0x4000249, 1, false, true, 2, (IoField[]){
		{"MST", 0, 2, TEXT_BOX},
		{"Enable", 7, 1, CHECKBOX}}},
	{"DIVCNT", "Division Control", 0x4000280, 2, true, true, 3, (IoField[]){
		{"Division Mode", 0, 2, SPECIAL},
		{"Division by zero", 14, 1, CHECKBOX},
		{"Busy", 15, 1, CHECKBOX}}},
	{"SQRTCNT", "Square Root Control", 0x40002B0, 2, true, true, 2, (IoField[]){
		{"Mode", 0, 1, SPECIAL},
		{"Busy", 15, 1, CHECKBOX}}},
	{"SQRT_RESULT", "Square Root Result", 0x40002B4, 4, true, false, 1, (IoField[]){
		{"Square Root Result", 0, 32, TEXT_BOX}}},
}};

void DebugMenu::ioReg9Window() { // Shamefully stolen from the ImGui demo
	static std::array<void *, 22> registerPointers9 = {
		&ortin.nds.ppu.DISPSTAT9,
		&ortin.nds.ppu.VCOUNT,
		&ortin.nds.shared.KEYINPUT,
		&ortin.nds.shared.KEYCNT9,
		&ortin.nds.ipc.IPCSYNC9,
		&ortin.nds.ipc.IPCFIFOCNT9,
		&ortin.nds.nds9.IME,
		&ortin.nds.nds9.IE,
		&ortin.nds.nds9.IF,
		&ortin.nds.ppu.VRAMCNT_A,
		&ortin.nds.ppu.VRAMCNT_B,
		&ortin.nds.ppu.VRAMCNT_C,
		&ortin.nds.ppu.VRAMCNT_D,
		&ortin.nds.ppu.VRAMCNT_E,
		&ortin.nds.ppu.VRAMCNT_F,
		&ortin.nds.ppu.VRAMCNT_G,
		&ortin.nds.shared.WRAMCNT,
		&ortin.nds.ppu.VRAMCNT_H,
		&ortin.nds.ppu.VRAMCNT_I,
		&ortin.nds.nds9.dsmath.DIVCNT,
		&ortin.nds.nds9.dsmath.SQRTCNT,
		&ortin.nds.nds9.dsmath.SQRT_RESULT,
	};

	static int selected = 0;
	static bool refresh = true;
	static bool tmpRefresh = false;

	ImGui::SetNextWindowSize(ImVec2(500, 440), ImGuiCond_FirstUseEver);
	ImGui::Begin("ARM9 IO Registers", &showIoReg9);

	// Left
	{
		ImGui::BeginChild("left pane", ImVec2(200, 0), true);
		for (int i = 0; i < registers9.size(); i++) {
			auto reg = registers9[i];
			if (ImGui::Selectable(fmt::format("{}: 0x{:0>7X} ({})", reg.name, reg.address, reg.readable == reg.writable ? "R/W" : (reg.readable ? "R" : "W")).c_str(), selected == i)) {
				if (i != selected)
					tmpRefresh = true;
				selected = i;
			}
		}
		ImGui::EndChild();
	}
	ImGui::SameLine();

	// Right
	int size = registers9[selected].size;
	u32 address = registers9[selected].address;
	static u32 value;
	{
		if (refresh || tmpRefresh) {
			if (size == 4) {
				value = *(u32 *)registerPointers9[selected];
			} else if (size == 2) {
				value = *(u16 *)registerPointers9[selected];
			} else {
				value = *(u8 *)registerPointers9[selected];
			}
		}

		ImGui::BeginGroup();
		ImGui::BeginChild("item view", ImVec2(0, -ImGui::GetFrameHeightWithSpacing())); // Leave room for 1 line below us

		ImGui::Text("%s", fmt::format("0x{:0>{}X} - {}", value, size * 2, registers9[selected].description).c_str());
		ImGui::Separator();

		for (int i = 0; i < registers9[selected].numFields; i++) {
			auto field = registers9[selected].fields[i];
			unsigned mask = ((1 << field.length) - 1) << field.startBit;
			u32 fValue = (value & mask) >> field.startBit;

			switch (field.type) {
			case TEXT_BOX:
				fValue = numberInput(((std::string)field.name).c_str(), false, fValue, mask >> field.startBit);
				break;
			case TEXT_BOX_HEX:
				fValue = numberInput(((std::string)field.name).c_str(), true, fValue, mask >> field.startBit);
				break;
			case CHECKBOX:
				ImGui::Checkbox(((std::string)field.name).c_str(), (bool *)&fValue);
				break;
			case SPECIAL:
				switch (address) {
				case 0x4000004: // DISPCNT
					fValue = numberInput(((std::string)field.name).c_str(), false, (fValue >> 1) | ((fValue & 1) << 8), mask >> field.startBit);
					fValue = ((fValue & 0xFF) << 1) | (fValue >> 8);
					break;
				case 0x4000132: { // KEYCNT
					const char *items[] = {"OR Mode",
										   "AND Mode"};
					ImGui::Combo("combo", (int *)&fValue, items, 2);
					} break;
				case 0x4000247: { // WAITCNT
					const char *items[] = {"Full 32KB",
										   "Top 16KB",
										   "Bottom 16KB",
										   "Unmapped"};
					ImGui::Combo("combo", (int *)&fValue, items, 4);
					} break;
				case 0x4000280: { // DIVCNT
					const char *items[] = {"32bit / 32bit = 32bit , 32bit",
										   "64bit / 32bit = 64bit , 32bit",
										   "64bit / 64bit = 64bit , 64bit",
										   "Reserved; same as Mode 1"};
					ImGui::Combo("combo", (int *)&fValue, items, 4);
					} break;
				case 0x40002B0: { // SQRTCNT
					const char *items[] = {"32bit input",
										   "64bit input"};
					ImGui::Combo("combo", (int *)&fValue, items, 2);
				} break;
				}
				break;
			}

			value = (value & ~mask) | (fValue << field.startBit);
		}
		ImGui::EndChild();

		ImGui::Checkbox("Refresh", &refresh);
		ImGui::SameLine();

		tmpRefresh = false;
		if (ImGui::Button("Refresh"))
			tmpRefresh = true;
		ImGui::SameLine();

		if (ImGui::Button("Write")) {
			if (size == 4) {
				ortin.nds.nds9.writeIO(address | 0, (u8)(value >> 0), false);
				ortin.nds.nds9.writeIO(address | 1, (u8)(value >> 8), false);
				ortin.nds.nds9.writeIO(address | 2, (u8)(value >> 16), false);
				ortin.nds.nds9.writeIO(address | 3, (u8)(value >> 24), true);
			} else if (size == 2) {
				ortin.nds.nds9.writeIO(address | 0, (u8)(value >> 0), false);
				ortin.nds.nds9.writeIO(address | 1, (u8)(value >> 8), true);
			} else {
				ortin.nds.nds9.writeIO(address, (u8)value, true);
			}

			tmpRefresh = true;
		}
		ImGui::EndGroup();
	}

	ImGui::End();
}

static const std::array<IoRegister, 11> registers7 = {{
	{"DISPSTAT", "Display Status and Interrupt Control", 0x4000004, 2, true, true, 7, (IoField[]){
		{"V-Blank", 0, 1, CHECKBOX},
		{"H-Blank", 1, 1, CHECKBOX},
		{"VCOUNT Compare Status", 2, 1, CHECKBOX},
		{"V-Blank IRQ", 3, 1, CHECKBOX},
		{"H-Blank IRQ", 4, 1, CHECKBOX},
		{"VCOUNT Compare IRQ", 5, 1, CHECKBOX},
		{"VCOUNT Compare Value", 7, 9, SPECIAL}}},
	{"VCOUNT", "Shows the Current Scanline", 0x4000006, 2, true, false, 1, (IoField[]){
		{"Current Scanline", 0, 9}}},
	{"KEYINPUT", "Key Status (Inverted)", 0x4000130, 2, true, false, 10, (IoField[]){
		{"A", 0, 1, CHECKBOX},
		{"B", 1, 1, CHECKBOX},
		{"Select", 2, 1, CHECKBOX},
		{"Start", 3, 1, CHECKBOX},
		{"Right", 4, 1, CHECKBOX},
		{"Left", 5, 1, CHECKBOX},
		{"Up", 6, 1, CHECKBOX},
		{"Down", 7, 1, CHECKBOX},
		{"R", 8, 1, CHECKBOX},
		{"L", 9, 1, CHECKBOX}}},
	{"KEYCNT", "Key Interrupt Control", 0x4000132, 2, true, true, 12, (IoField[]){
		{"A", 0, 1, CHECKBOX},
		{"B", 1, 1, CHECKBOX},
		{"Select", 2, 1, CHECKBOX},
		{"Start", 3, 1, CHECKBOX},
		{"Right", 4, 1, CHECKBOX},
		{"Left", 5, 1, CHECKBOX},
		{"Up", 6, 1, CHECKBOX},
		{"Down", 7, 1, CHECKBOX},
		{"R", 8, 1, CHECKBOX},
		{"L", 9, 1, CHECKBOX},
		{"IRQ Enable", 14, 1, CHECKBOX},
		{"Condition", 15, 1, SPECIAL}}},
	{"EXTKEYIN", "Key X/Y Input (Inverted)", 0x4000136, 2, true, false, 5, (IoField[]){
		{"X", 0, 1, CHECKBOX},
		{"Y", 1, 1, CHECKBOX},
		{"DEBUG button", 3, 1, CHECKBOX},
		{"Pen down", 6, 1, CHECKBOX},
		{"Hinge open", 7, 1, CHECKBOX}}},
	{"IPCSYNC", "IPC Synchronize Register", 0x4000180, 2, true, true, 4, (IoField[]){
		{"Data input from IPCSYNC of remote CPU", 0, 4, TEXT_BOX_HEX},
		{"Data output to IPCSYNC of remote CPU", 8, 4, TEXT_BOX_HEX},
		{"Send IRQ to remote CPU", 13, 1, CHECKBOX},
		{"Enable IRQ from remote CPU", 14, 1, CHECKBOX}}},
	{"IPCFIFOCNT", "IPC Fifo Control Register", 0x4000184, 2, true, true, 9, (IoField[]){
		{"Send Fifo Empty Status", 0, 1, CHECKBOX},
		{"Send Fifo Full Status", 1, 1, CHECKBOX},
		{"Send Fifo Empty IRQ", 2, 1, CHECKBOX},
		{"Send Fifo Clear", 3, 1, CHECKBOX},
		{"Receive Fifo Empty Status", 8, 1, CHECKBOX},
		{"Receive Fifo Full Status", 9, 1, CHECKBOX},
		{"Receive Fifo Not Empty IRQ", 10, 1, CHECKBOX},
		{"Error, Read Empty/Send Full", 14, 1, CHECKBOX},
		{"Enable Send/Receive Fifo", 15, 1, CHECKBOX}}},
	{"IME", "Interrupt Master Enable", 0x4000208, 4, true, true, 1, (IoField[]){
		{"Enable Interrupts", 0, 1, CHECKBOX}}},
	{"IE", "Interrupt Enable", 0x4000210, 4, true, true, 22, (IoField[]){
		{"LCD V-BLank", 0,  1, CHECKBOX},
		{"LCD H-BLank", 1,  1, CHECKBOX},
		{"LCD V-Counter Match", 2,  1, CHECKBOX},
		{"Timer 0 Overflow", 3,  1, CHECKBOX},
		{"Timer 1 Overflow", 4,  1, CHECKBOX},
		{"Timer 2 Overflow", 5,  1, CHECKBOX},
		{"Timer 3 Overflow", 6,  1, CHECKBOX},
		{"SIO/RCNT/RTC", 7,  1, CHECKBOX},
		{"DMA 0", 8,  1, CHECKBOX},
		{"DMA 1", 9,  1, CHECKBOX},
		{"DMA 2", 10, 1, CHECKBOX},
		{"DMA 3", 11, 1, CHECKBOX},
		{"Keypad", 12, 1, CHECKBOX},
		{"GBA Slot", 13, 1, CHECKBOX},
		{"IPC Sync",  16, 1, CHECKBOX},
		{"IPC Send FIFO Empty", 17, 1, CHECKBOX},
		{"IPC Recv FIFO Not Empty", 18, 1, CHECKBOX},
		{"NDS-Slot Game Card Data Transfer Completion", 19, 1, CHECKBOX},
		{"NDS-Slot Game Card IREQ_MC", 20, 1, CHECKBOX},
		{"Screens unfolding", 22, 1, CHECKBOX},
		{"SPI bus", 23, 1, CHECKBOX},
		{"Wifi", 24, 1, CHECKBOX}}},
	{"IF", "Interrupt Request Flags", 0x4000214, 4, true, true, 22, (IoField[]){
		{"LCD V-BLank", 0,  1, CHECKBOX},
		{"LCD H-BLank", 1,  1, CHECKBOX},
		{"LCD V-Counter Match", 2,  1, CHECKBOX},
		{"Timer 0 Overflow", 3,  1, CHECKBOX},
		{"Timer 1 Overflow", 4,  1, CHECKBOX},
		{"Timer 2 Overflow", 5,  1, CHECKBOX},
		{"Timer 3 Overflow", 6,  1, CHECKBOX},
		{"SIO/RCNT/RTC", 7,  1, CHECKBOX},
		{"DMA 0", 8,  1, CHECKBOX},
		{"DMA 1", 9,  1, CHECKBOX},
		{"DMA 2", 10, 1, CHECKBOX},
		{"DMA 3", 11, 1, CHECKBOX},
		{"Keypad", 12, 1, CHECKBOX},
		{"GBA Slot", 13, 1, CHECKBOX},
		{"IPC Sync",  16, 1, CHECKBOX},
		{"IPC Send FIFO Empty", 17, 1, CHECKBOX},
		{"IPC Recv FIFO Not Empty", 18, 1, CHECKBOX},
		{"NDS-Slot Game Card Data Transfer Completion", 19, 1, CHECKBOX},
		{"NDS-Slot Game Card IREQ_MC", 20, 1, CHECKBOX},
		{"Screens unfolding", 22, 1, CHECKBOX},
		{"SPI bus", 23, 1, CHECKBOX},
		{"Wifi", 24, 1, CHECKBOX}}},
	{"WRAMSTAT", "WRAM Bank Status", 0x4000241, 1, true, false, 1, (IoField[]){
		{"Mapping", 0, 2, SPECIAL}}},
}};

void DebugMenu::ioReg7Window() {
	static std::array<void *, 11> registerPointers7 = {
		&ortin.nds.ppu.DISPSTAT7,
		&ortin.nds.ppu.VCOUNT,
		&ortin.nds.shared.KEYINPUT,
		&ortin.nds.shared.KEYCNT7,
		&ortin.nds.shared.EXTKEYIN,
		&ortin.nds.ipc.IPCSYNC7,
		&ortin.nds.ipc.IPCFIFOCNT7,
		&ortin.nds.nds7.IME,
		&ortin.nds.nds7.IE,
		&ortin.nds.nds7.IF,
		&ortin.nds.shared.WRAMCNT
	};

	static int selected = 0;
	static bool refresh = true;
	static bool tmpRefresh = false;

	ImGui::SetNextWindowSize(ImVec2(500, 440), ImGuiCond_FirstUseEver);
	ImGui::Begin("ARM7 IO Registers", &showIoReg7);

	// Left
	{
		ImGui::BeginChild("left pane", ImVec2(200, 0), true);
		for (int i = 0; i < registers7.size(); i++) {
			auto reg = registers7[i];
			if (ImGui::Selectable(fmt::format("{}: 0x{:0>7X} ({})", reg.name, reg.address, reg.readable == reg.writable ? "R/W" : (reg.readable ? "R" : "W")).c_str(), selected == i)) {
				if (i != selected)
					tmpRefresh = true;
				selected = i;
			}
		}
		ImGui::EndChild();
	}
	ImGui::SameLine();

	// Right
	int size = registers7[selected].size;
	u32 address = registers7[selected].address;
	static u32 value;
	{
		if (refresh || tmpRefresh) {
			if (size == 4) {
				value = *(u32 *)registerPointers7[selected];
			} else if (size == 2) {
				value = *(u16 *)registerPointers7[selected];
			} else {
				value = *(u8 *)registerPointers7[selected];
			}
		}

		ImGui::BeginGroup();
		ImGui::BeginChild("item view", ImVec2(0, -ImGui::GetFrameHeightWithSpacing())); // Leave room for 1 line below us

		ImGui::Text("%s", fmt::format("0x{:0>{}X} - {}", value, size * 2, registers7[selected].description).c_str());
		ImGui::Separator();

		for (int i = 0; i < registers7[selected].numFields; i++) {
			auto field = registers7[selected].fields[i];
			unsigned mask = ((1 << field.length) - 1) << field.startBit;
			u32 fValue = (value & mask) >> field.startBit;

			switch (field.type) {
			case TEXT_BOX:
				fValue = numberInput(((std::string)field.name).c_str(), false, fValue, mask >> field.startBit);
				break;
			case TEXT_BOX_HEX:
				fValue = numberInput(((std::string)field.name).c_str(), true, fValue, mask >> field.startBit);
				break;
			case CHECKBOX:
				ImGui::Checkbox(((std::string)field.name).c_str(), (bool *)&fValue);
				break;
			case SPECIAL:
				switch (address) {
				case 0x4000004: // DISPCNT
					fValue = numberInput(((std::string)field.name).c_str(), false, (fValue >> 1) | ((fValue & 1) << 8), mask >> field.startBit);
					fValue = ((fValue & 0xFF) << 1) | (fValue >> 8);
					break;
				case 0x4000241: { // WAITSTAT
					const char *items[] = {"Unmapped/WRAM Mirror", "Bottom 16KB", "Top 16KB", "Full 32KB"};
					ImGui::Combo("combo", (int *)&fValue, items, 4);
					} break;
				}
				break;
			}

			value = (value & ~mask) | (fValue << field.startBit);
		}
		ImGui::EndChild();

		ImGui::Checkbox("Refresh", &refresh);
		ImGui::SameLine();

		tmpRefresh = false;
		if (ImGui::Button("Refresh"))
			tmpRefresh = true;
		ImGui::SameLine();

		if (ImGui::Button("Write")) {
			if (size == 4) {
				ortin.nds.nds7.writeIO(address | 0, (u8)(value >> 0), false);
				ortin.nds.nds7.writeIO(address | 1, (u8)(value >> 8), false);
				ortin.nds.nds7.writeIO(address | 2, (u8)(value >> 16), false);
				ortin.nds.nds7.writeIO(address | 3, (u8)(value >> 24), true);
			} else if (size == 2) {
				ortin.nds.nds7.writeIO(address | 0, (u8)(value >> 0), false);
				ortin.nds.nds7.writeIO(address | 1, (u8)(value >> 8), true);
			} else {
				ortin.nds.nds7.writeIO(address, (u8)value, true);
			}

			tmpRefresh = true;
		}
		ImGui::EndGroup();
	}

	ImGui::End();
}
