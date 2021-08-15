// https://cpprefjp.github.io/lang/cpp20/coroutines.html
// https://logicalbeat.jp/blog/5014/

#include <iostream>
#include <utility>

#include <experimental/resumable>
#include <experimental/coroutine>
namespace std
{
using std::experimental::coroutine_handle;
using std::experimental::suspend_always;
using std::experimental::suspend_never;

} // namespace std

// コルーチン利用ライブラリ: ジェネレータ型
struct generator
{
    // ジェネレータに関連付けられるPromise型
    struct promise_type
    {
        // co_yield式で指定されるint値を保持する変数
        int value_;

        auto get_return_object()
        {
            // コルーチンに紐づくPromiseオブジェクト(*this)から
            // ジェネレータ型のコルーチン戻り値オブジェクトを生成
            return generator{*this};
        };
        auto initial_suspend()
        {
            // コルーチン本体処理の開始前に無条件サスペンド
            return std::suspend_always{};
        }
        auto final_suspend()
        {
            // コルーチン本体処理の終了後に無条件サスペンド
            return std::suspend_always{};
        }
        auto yield_value(int v)
        {
            // co_yield式で渡される値を保持し、コルーチンを無条件サスペンド
            value_ = v;
            return std::suspend_always{};
        }
        void return_void()
        {
        }
        void unhandled_exception()
        {
            std::terminate();
        }
    };
    // ジェネレータに関連付けられるコルーチンハンドル型
    using coro_handle = std::coroutine_handle<promise_type>;

    // 範囲for構文サポート用イテレータ型
    struct iterator
    {
        // 対象のコルーチンハンドル
        coro_handle coro_;
        // 対象コルーチン本体処理が終了したかを表すフラグ
        bool done_;

        iterator &operator++()
        {
            // yield_value()で中断したコルーチンを再開する
            coro_.resume();
            // (co_yield式評価もしくはコルーチン本体処理の終了により制御が戻ってくる)
            done_ = coro_.done();
            return *this;
        }
        bool operator!=(const iterator &rhs) const
        {
            return done_ != rhs.done_;
        }
        int operator*() const
        {
            // Promiseオブジェクトが保持している値を返す
            return coro_.promise().value_;
        }
    };

    ~generator()
    {
        if (coro_)
            coro_.destroy();
    }

    generator(generator const &) = delete;
    generator(generator &&rhs) : coro_(std::exchange(rhs.coro_, nullptr))
    {
    }

    // 範囲for構文サポート用のメンバ関数
    iterator begin()
    {
        // initial_suspend()で中断したコルーチンを再開する
        coro_.resume();
        // (初回co_yield式評価により制御が戻ってくる)
        return {coro_, coro_.done()};
    }
    iterator end()
    {
        // 終端位置を表現する番兵イテレータ
        return {{}, true};
    }

private:
    // Promiseオブジェクト経由でコルーチンハンドルを取得する
    explicit generator(promise_type &p) : coro_(coro_handle::from_promise(p))
    {
    }

    coro_handle coro_;
};

// ユーザ定義コルーチン
generator iota(int end)
{
    // コルーチンに対応したPromise型 generator::promise_typeの
    // Promiseオブジェクト(p)が生成される。

    for (int n = 0; n < end; ++n)
    {
        // 下式は co_await p.yield_value(n) と等価
        co_yield n;
    }
    // コルーチン本体の終端到達により p.return_void() 呼び出し
}

int main()
{
    // コルーチンを呼び出し、整数生成ジェネレータを取得する。
    auto g = iota(10);
    // このタイミングではまだコルーチン本体は実行されない。

    // 範囲for構文を用いてコルーチン本体を実行する。
    // ここではコルーチンiotaの値生成ループ処理ステップと、
    // main関数の表示ループ処理ステップが交互に実行される。
    for (int v : g)
    {
        std::cout << v;
    }
}
