#pragma once
// helper type for the visitor
template<class... Ts>
struct overloads : Ts... { using Ts::operator()...; };