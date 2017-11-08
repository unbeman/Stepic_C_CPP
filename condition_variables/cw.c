#include <assert.h>
#include <stddef.h>

/* Напоминание, как выглядят интерфейсы блокировки и переменной
   состояния.

   ВАЖНО: обратите внимание на функции lock_init и condition_init,
          я не уделял этому внимание в видео, но блокировки и
          переменные состояния нужно инциализировать.
*/

struct lock;
void lock_init(struct lock *lock);
void lock(struct lock *lock);
void unlock(struct lock *lock);

struct condition;
void condition_init(struct condition *cv);
void wait(struct condition *cv, struct lock *lock);
void notify_one(struct condition *cv);
void notify_all(struct condition *cv);



/* Далее следует интерфейс, который вам нужно реализовать.

   ВАЖНО: в шаблоне кода стукрутуры содержат поля, некоторые
          функции уже реализованы и присутсвуют комментарии
          - это не более чем подсказка. Вы можете игнорировать
          комментарии, изменять поля структур и реализации
          функций на ваше усмотрение, при условии, что вы
          сохранили интерфейс.

          Вам нельзя изменять имена структур (wdlock_ctx и
          wdlock), а также имена функций (wdlock_ctx_init,
          wdlock_init, wdlock_lock, wdlock_unlock).
*/

struct wdlock_ctx;

struct wdlock {
    /* wdlock_ctx должен хранить информацию обо всех
       захваченных wdlock-ах, а это поле позволит связать
       wdlock-и в список. */
    struct wdlock *next;

    /* Текущий владелец блокировки - из него мы извлекаем
       timestamp связанный с блокировкой, если блокировка
       свободна, то хранит NULL. */
    const struct wdlock_ctx *owner;

    /* lock и cv могут быть использованы чтобы дождаться
       пока блокировка не освободится либо у нее не сменится
       владелец. */
    struct lock lock;
    struct condition cv;
};


/* Каждый контекст имеет свой уникальный timestamp и хранит
   список захваченных блокировок. */
struct wdlock_ctx {
    unsigned long long timestamp;
    struct wdlock *locks;
};


/* Всегда вызывается перед тем, как использовать контекст.

   ВАЖНО: функция является частью интерфейса - не меняйте
          ее имя и аргументы.
*/
void wdlock_ctx_init(struct wdlock_ctx *ctx)
{
    static atomic_ullong next;

    ctx->timestamp = atomic_fetch_add(&next, 1) + 1;
    ctx->locks = NULL;
}

/* Всегда вызывается перед тем, как использовать блокировку.

   ВАЖНО: функция является частью интерфейса - не меняйте
          ее имя и аргументы.
*/
void wdlock_init(struct wdlock *lock)
{
    lock_init(&lock->lock);
    condition_init(&lock->cv);
    lock->owner = NULL;
}

/* Функция для захвата блокировки current контекстом ctx. Если
   захват блокировки прошел успешно функция должна вернуть
   ненулевое значение. Если же захват блокировки провалился
   из-за проверки timestamp-а, то функция должна вернуть 0.

   Помните, что контекст должен хранить информацию о
   захваченных блокировках, чтобы иметь возможность освободить
   их в функции wdlock_unlock.

   ВАЖНО: функция является частью интерфейса - не меняйте
          ее имя и аргументы.
*/

void wdlock_unlock(struct wdlock_ctx *ctx)
{
    assert(ctx);
    struct  wdlock *current = ctx->locks;

    while (current != NULL) {
        assert(current->owner == ctx);
        current->owner = NULL;
        notify_all(&current->cv);
        unlock(&current->lock);
        current = current->next;
    }
}

int wdlock_lock(struct wdlock *l, struct wdlock_ctx *ctx) {
    assert(l);
    assert(ctx);
    lock(&l->lock);
    if (l->owner  && ctx->timestamp < l->owner->timestamp) {
        while (l->owner &&  l->owner->timestamp < ctx->timestamp ) {
            wait(&l->cv, &l->lock);
        }
    }
    if (l->owner == NULL) {
        l->owner = ctx;

        struct wdlock *tmp = ctx->locks;
        ctx->locks = l;
        ctx->locks->next = tmp;
        notify_one(&l->cv);
        unlock(&l->lock);
        return 1;
    }
    else { //if (current->owner != NULL && ctx->timestamp >= current->owner->timestamp)
        notify_one(&l->cv);
        wdlock_unlock(ctx);
        unlock(&l->lock);
        return 0;
    }
}

/* Функция для освбождения всех блокировок, захваченных
   контекстом ctx. При этом может случиться так, что этот
   контекст не владеет ни одной блокировкой, в этом случае
   ничего делать не нужно.

   ВАЖНО: функция является частью интерфейса - не меняйте
          ее имя и аргументы.
*/

