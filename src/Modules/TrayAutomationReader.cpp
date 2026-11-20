/**
 * @file TrayAutomationReader.cpp
 *
 * @version 1.0.0
 * @author BleckWolf25
 * @license MIT
 *
 * @summary Implementation of the UI Automation notification area backend
 *
 * @description
 * Creates IUIAutomation per call, resolves Shell_TrayWnd and the overflow island, and walks
 * button controls with a ControlType condition. Each element supplies a name used both as the
 * tooltip and as the persistence key. Entries carry no native callback window, so TrayTracker
 * routes their activation through IUIAutomationInvokePattern. Every COM pointer is ComPtr-owned
 * and each HRESULT is checked, so a missing automation tree degrades to an empty list.
 *
 * @since 18/11/2026
 * @updated 18/11/2026
 */
// ---------- IMPORTS
#include "Modules/TrayAutomationReader.h"
#include "Core/WinString.h"
#include <Windows.h>
#include <objbase.h>
#include <oleauto.h>
#include <UIAutomation.h>
#include <wrl/client.h>
#include <unordered_set>

// ---------- CONSTANTS
namespace {
constexpr wchar_t kTrayWindowClass[] = L"Shell_TrayWnd";
constexpr wchar_t kOverflowWindowClass[] = L"TopLevelWindowForOverflowXamlIsland";

using AutomationPtr = Microsoft::WRL::ComPtr<IUIAutomation>;
using ElementPtr = Microsoft::WRL::ComPtr<IUIAutomationElement>;
using ArrayPtr = Microsoft::WRL::ComPtr<IUIAutomationElementArray>;
using ConditionPtr = Microsoft::WRL::ComPtr<IUIAutomationCondition>;

AutomationPtr createAutomation() {
    AutomationPtr automation;
    if (FAILED(CoCreateInstance(__uuidof(CUIAutomation), nullptr, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(automation.GetAddressOf())))) {
        return nullptr;
    }
    return automation;
}

std::wstring elementName(IUIAutomationElement* element) {
    BSTR raw = nullptr;
    if (FAILED(element->get_CurrentName(&raw)) || !raw) {
        return {};
    }
    std::wstring name(raw, SysStringLen(raw));
    SysFreeString(raw);
    return name;
}

void collectFromWindow(IUIAutomation* automation, HWND root,
                       std::unordered_set<std::wstring>& seen,
                       std::vector<RawTrayIcon>& out) {
    if (!root) {
        return;
    }
    ElementPtr element;
    if (FAILED(automation->ElementFromHandle(root, element.GetAddressOf())) || !element) {
        return;
    }
    VARIANT variant{};
    variant.vt = VT_I4;
    variant.lVal = UIA_ButtonControlTypeId;
    ConditionPtr condition;
    if (FAILED(automation->CreatePropertyCondition(UIA_ControlTypePropertyId, variant,
                                                   condition.GetAddressOf()))) {
        return;
    }
    ArrayPtr buttons;
    if (FAILED(element->FindAll(TreeScope_Descendants, condition.Get(),
                                buttons.GetAddressOf())) ||
        !buttons) {
        return;
    }
    int count = 0;
    if (FAILED(buttons->get_Length(&count))) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        ElementPtr button;
        if (FAILED(buttons->GetElement(i, button.GetAddressOf())) || !button) {
            continue;
        }
        const std::wstring name = elementName(button.Get());
        if (name.empty()) {
            continue;
        }
        RawTrayIcon raw{};
        raw.source = TrayIconSource::Automation;
        raw.ownerHwnd = nullptr;
        raw.tooltip = name;
        raw.persistKey = L"uia:" + toLowerWide(name);
        if (seen.insert(raw.persistKey).second) {
            out.push_back(std::move(raw));
        }
    }
}

void collectAll(IUIAutomation* automation, std::vector<RawTrayIcon>& out) {
    std::unordered_set<std::wstring> seen;
    collectFromWindow(automation, FindWindowW(kTrayWindowClass, nullptr), seen, out);
    collectFromWindow(automation, FindWindowW(kOverflowWindowClass, nullptr), seen, out);
}
}
// ---------- IMPLEMENTATION
std::vector<RawTrayIcon> readAutomationTrayIcons() {
    std::vector<RawTrayIcon> icons;
    const AutomationPtr automation = createAutomation();
    if (!automation) {
        return icons;
    }
    collectAll(automation.Get(), icons);
    return icons;
}

bool invokeAutomationTrayIcon(const std::wstring& persistKey) {
    const AutomationPtr automation = createAutomation();
    if (!automation) {
        return false;
    }
    std::vector<RawTrayIcon> icons;
    collectAll(automation.Get(), icons);
    std::wstring target;
    for (const auto& icon : icons) {
        if (icon.persistKey == persistKey) {
            target = icon.tooltip;
            break;
        }
    }
    if (target.empty()) {
        return false;
    }
    VARIANT variant{};
    variant.vt = VT_BSTR;
    variant.bstrVal = SysAllocString(target.c_str());
    if (!variant.bstrVal) {
        return false;
    }
    ConditionPtr condition;
    const HRESULT created = automation->CreatePropertyCondition(UIA_NamePropertyId, variant,
                                                                condition.GetAddressOf());
    SysFreeString(variant.bstrVal);
    if (FAILED(created)) {
        return false;
    }
    const HWND roots[2] = {FindWindowW(kTrayWindowClass, nullptr),
                           FindWindowW(kOverflowWindowClass, nullptr)};
    for (HWND root : roots) {
        if (!root) {
            continue;
        }
        ElementPtr element;
        if (FAILED(automation->ElementFromHandle(root, element.GetAddressOf())) || !element) {
            continue;
        }
        ElementPtr found;
        if (FAILED(element->FindFirst(TreeScope_Descendants, condition.Get(),
                                      found.GetAddressOf())) ||
            !found) {
            continue;
        }
        Microsoft::WRL::ComPtr<IUIAutomationInvokePattern> invoke;
        if (SUCCEEDED(found->GetCurrentPatternAs(UIA_InvokePatternId,
                                                 IID_PPV_ARGS(invoke.GetAddressOf()))) &&
            invoke && SUCCEEDED(invoke->Invoke())) {
            return true;
        }
    }
    return false;
}