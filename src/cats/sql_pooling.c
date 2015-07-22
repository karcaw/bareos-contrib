/*
   BAREOS® - Backup Archiving REcovery Open Sourced

   Copyright (C) 2010-2012 Free Software Foundation Europe e.V.
   Copyright (C) 2011-2012 Planets Communications B.V.
   Copyright (C) 2013-2013 Bareos GmbH & Co. KG

   This program is Free Software; you can redistribute it and/or
   modify it under the terms of version three of the GNU Affero General Public
   License as published by the Free Software Foundation and included
   in the file LICENSE.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
   Affero General Public License for more details.

   You should have received a copy of the GNU Affero General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
   02110-1301, USA.
*/
/*
 * BAREOS sql pooling code that manages the database connection pools.
 *
 * Written by Marco van Wieringen, March 2010
 */

#include "bareos.h"

#if HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI

#include "cats.h"
#include "bdb_priv.h"
#include "sql_glue.h"

/*
 * Get a non-pooled connection used when either sql pooling is
 * runtime disabled or at compile time. Or when we run out of
 * pooled connections and need more database connections.
 */
B_DB *db_sql_get_non_pooled_connection(JCR *jcr,
                                       const char *db_drivername,
                                       const char *db_name,
                                       const char *db_user,
                                       const char *db_password,
                                       const char *db_address,
                                       int db_port,
                                       const char *db_socket,
                                       bool mult_db_connections,
                                       bool disable_batch_insert,
                                       bool need_private,
                                       bool try_reconnect,
                                       bool exit_on_fatal)
{
   B_DB *mdb;

#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
   Dmsg2(100, "db_sql_get_non_pooled_connection allocating 1 new non pooled database connection to database %s, backend type %s\n",
         db_name, db_drivername);
#else
   Dmsg1(100, "db_sql_get_non_pooled_connection allocating 1 new non pooled database connection to database %s\n",
         db_name);
#endif
   mdb = db_init_database(jcr,
                          db_drivername,
                          db_name,
                          db_user,
                          db_password,
                          db_address,
                          db_port,
                          db_socket,
                          mult_db_connections,
                          disable_batch_insert,
                          need_private,
                          try_reconnect,
                          exit_on_fatal);
   if (mdb == NULL) {
      return NULL;
   }

   if (!db_open_database(jcr, mdb)) {
      Mmsg2(mdb->errmsg, _("Could not open database \"%s\": ERR=%s\n"), db_name, db_strerror(mdb));
      Jmsg(jcr, M_FATAL, 0, "%s", mdb->errmsg);
      db_close_database(jcr, mdb);
      return NULL;
   }

   return mdb;
}

#ifdef HAVE_SQL_POOLING

static dlist *db_pooling_descriptors = NULL;

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void destroy_pool_descriptor(SQL_POOL_DESCRIPTOR *spd, bool flush_only)
{
   SQL_POOL_ENTRY *spe, *spe_next;

   spe = (SQL_POOL_ENTRY *)spd->pool_entries->first();
   while (spe) {
      spe_next = (SQL_POOL_ENTRY *)spd->pool_entries->get_next(spe);
      if (!flush_only || spe->reference_count == 0) {
         Dmsg3(100, "db_sql_pool_destroy destroy db pool connection %d to %s, backend type %s\n",
               spe->id, spe->db_handle->get_db_name(), spe->db_handle->db_get_type());
         db_close_database(NULL, spe->db_handle);
         if (flush_only) {
            spd->pool_entries->remove(spe);
            free(spe);
         }
         spd->nr_connections--;
      }
      spe = spe_next;
   }

   /*
    * See if there is anything left on this pool and we are flushing the pool.
    */
   if (flush_only && spd->nr_connections == 0) {
      db_pooling_descriptors->remove(spd);
      delete spd->pool_entries;
      free(spd);
   }
}

/*
 * Initialize the sql connection pool.
 */
bool db_sql_pool_initialize(const char *db_drivername,
                            const char *db_name,
                            const char *db_user,
                            const char *db_password,
                            const char *db_address,
                            int db_port,
                            const char *db_socket,
                            bool disable_batch_insert,
                            bool try_reconnect,
                            bool exit_on_fatal,
                            int min_connections,
                            int max_connections,
                            int increment_connections,
                            int idle_timeout,
                            int validate_timeout)
{
   int cnt;
   B_DB *mdb;
   time_t now;
   SQL_POOL_DESCRIPTOR *spd = NULL;
   SQL_POOL_ENTRY *spe = NULL;
   bool retval = false;

   /*
    * See if pooling is runtime disabled.
    */
   if (max_connections == 0) {
      Dmsg0(100, "db_sql_pool_initialize pooling disabled as max_connections == 0\n");
      return true;
   }

   /*
    * First make sure the values make any sense.
    */
   if (min_connections <= 0 ||
       max_connections <= 0 ||
       increment_connections <= 0 ||
       min_connections > max_connections) {
      Jmsg(NULL, M_FATAL, 0,
           _("Illegal values for sql pool initialization, min_connections = %d, max_connections = %d, increment_connections = %d"),
           min_connections, max_connections, increment_connections);
      return false;
   }

   P(mutex);
   time(&now);

   if (db_pooling_descriptors == NULL) {
      db_pooling_descriptors = New(dlist(spd, &spd->link));
   }

   /*
    * Create a new pool descriptor.
    */
   spd = (SQL_POOL_DESCRIPTOR *)malloc(sizeof(SQL_POOL_DESCRIPTOR));
   memset(spd, 0, sizeof(SQL_POOL_DESCRIPTOR));
   spd->pool_entries = New(dlist(spe, &spe->link));
   spd->min_connections = min_connections;
   spd->max_connections = max_connections;
   spd->increment_connections = increment_connections;
   spd->idle_timeout = idle_timeout;
   spd->validate_timeout = validate_timeout;
   spd->last_update = now;
   spd->active = true;

   /*
    * Create a number of database connections.
    */
   for (cnt = 0; cnt < min_connections; cnt++) {
      mdb = db_init_database(NULL,
                             db_drivername,
                             db_name,
                             db_user,
                             db_password,
                             db_address,
                             db_port,
                             db_socket,
                             true,
                             disable_batch_insert,
                             try_reconnect,
                             exit_on_fatal);
      if (mdb == NULL) {
         Jmsg(NULL, M_FATAL, 0, "%s", _("Could not init database connection"));
         goto bail_out;
      }

      if (!db_open_database(NULL, mdb)) {
         Mmsg2(mdb->errmsg, _("Could not open database \"%s\": ERR=%s\n"),
               db_name, db_strerror(mdb));
         Jmsg(NULL, M_FATAL, 0, "%s", mdb->errmsg);
         db_close_database(NULL, mdb);
         goto bail_out;
      }

      /*
       * Push this new connection onto the connection pool.
       */
      spe = (SQL_POOL_ENTRY *)malloc(sizeof(SQL_POOL_ENTRY));
      memset(spe, 0, sizeof(SQL_POOL_ENTRY));
      spe->id = spd->nr_connections++;
      spe->last_update = now;
      spe->db_handle = mdb;
      spd->pool_entries->append(spe);
      spe = NULL;
   }

#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
   Dmsg3(100, "db_sql_pool_initialize created %d connections to database %s, backend type %s\n",
         cnt, db_name, db_drivername);
#else
   Dmsg2(100, "db_sql_pool_initialize created %d connections to database %s\n",
         cnt, db_name);
#endif
   db_pooling_descriptors->append(spd);
   retval = true;
   goto ok_out;

bail_out:
   if (spe) {
      free(spe);
   }

   if (spd) {
      destroy_pool_descriptor(spd, false);
   }

ok_out:
   V(mutex);
   return retval;
}

/*
 * Cleanup the sql connection pools.
 * This gets called on shutdown.
 */
void db_sql_pool_destroy(void)
{
   SQL_POOL_DESCRIPTOR *spd, *spd_next;

   /*
    * See if pooling is enabled.
    */
   if (!db_pooling_descriptors) {
      return;
   }

   P(mutex);
   spd = (SQL_POOL_DESCRIPTOR *)db_pooling_descriptors->first();
   while (spd) {
      spd_next = (SQL_POOL_DESCRIPTOR *)db_pooling_descriptors->get_next(spd);
      destroy_pool_descriptor(spd, false);
      spd = spd_next;
   }
   delete db_pooling_descriptors;
   db_pooling_descriptors = NULL;
   V(mutex);
}

/*
 * Flush the sql connection pools.
 * This gets called on config reload. We close all unreferenced connections.
 */
void db_sql_pool_flush(void)
{
   SQL_POOL_ENTRY *spe;
   SQL_POOL_DESCRIPTOR *spd, *spd_next;

   /*
    * See if pooling is enabled.
    */
   if (!db_pooling_descriptors) {
      return;
   }

   P(mutex);
   spd = (SQL_POOL_DESCRIPTOR *)db_pooling_descriptors->first();
   while (spd) {
      spd_next = (SQL_POOL_DESCRIPTOR *)db_pooling_descriptors->get_next(spd);
      if (spd->active) {
         /*
          * On a flush all current available pools are invalidated.
          */
         spd->active = false;
         destroy_pool_descriptor(spd, true);
      }
      spd = spd_next;
   }
   V(mutex);
}

/*
 * Grow the sql connection pool.
 * This function should be called with the mutex held.
 */
static inline void sql_pool_grow(SQL_POOL_DESCRIPTOR *spd)
{
   int cnt, next_id;
   B_DB *mdb;
   time_t now;
   SQL_POOL_ENTRY *spe;
   B_DB *db_handle;

   /*
    * Get the first entry from the list to be able to clone it.
    * If the pool is empty its not initialized ok so we cannot really
    * grow its size.
    */
   spe = (SQL_POOL_ENTRY *)spd->pool_entries->first();
   if (spe != NULL) {
      /*
       * Save the handle of the first entry so we can clone it later on.
       */
      db_handle = spe->db_handle;

      /*
       * Now that the pool is about to be grown give each entry a new id.
       */
      cnt = 0;
      foreach_dlist(spe, spd->pool_entries) {
         spe->id = cnt++;
      }

      /*
       * Remember the next available id to use.
       */
      next_id = cnt;

      /*
       * Create a number of database connections.
       */
      time(&now);
      for (cnt = 0; cnt < spd->increment_connections; cnt++) {
         /*
          * Get a new non-pooled connection to the database.
          * We want to add a non pooled connection to the pool as otherwise
          * we are creating a deadlock as db_clone_database_connection will
          * call sql_pool_get_connection which means a recursive enter into
          * the pooling code and as such the mutex will deadlock.
          */
         mdb = db_clone_database_connection(db_handle, NULL, true, false);
         if (mdb == NULL) {
            Jmsg(NULL, M_FATAL, 0, "%s", _("Could not init database connection"));
            break;
         }

         /*
          * Push this new connection onto the connection pool.
          */
         spe = (SQL_POOL_ENTRY *)malloc(sizeof(SQL_POOL_ENTRY));
         memset(spe, 0, sizeof(SQL_POOL_ENTRY));
         spe->id = next_id++;
         spe->last_update = now;
         spe->db_handle = mdb;
         spd->pool_entries->append(spe);
      }
      Dmsg3(100, "sql_pool_grow created %d connections to database %s, backend type %s\n",
            cnt, spe->db_handle->get_db_name(), spe->db_handle->db_get_type());
      spd->last_update = now;
   } else {
      Dmsg0(100, "sql_pool_grow unable to determine first entry on pool list\n");
   }
}

/*
 * Shrink the sql connection pool.
 * This function should be called with the mutex held.
 */
static inline void sql_pool_shrink(SQL_POOL_DESCRIPTOR *spd)
{
   int cnt;
   time_t now;
   SQL_POOL_ENTRY *spe, *spe_next;

   time(&now);
   spd->last_update = now;

   /*
    * See if we want to shrink.
    */
   if (spd->min_connections && spd->nr_connections <= spd->min_connections) {
      Dmsg0(100, "sql_pool_shrink cannot shrink connection pool already minimum size\n");
      return;
   }

   /*
    * See how much we should shrink.
    * No need to shrink under min_connections, and when things are greater
    * shrink with increment_connections per shrink run.
    */
   cnt = spd->nr_connections - spd->min_connections;
   if (cnt > spd->increment_connections) {
      cnt = spd->increment_connections;
   }

   /*
    * Sanity check.
    */
   if (cnt <= 0) {
      return;
   }

   /*
    * For debugging purposes get the first entry on the connection pool.
    */
   spe = (SQL_POOL_ENTRY *)spd->pool_entries->first();
   if (spe) {
      Dmsg3(100, "sql_pool_shrink shrinking connection pool with %d connections to database %s, backend type %s\n",
            cnt, spe->db_handle->get_db_name(), spe->db_handle->db_get_type());
   }

   /*
    * Loop over all entries on the pool and see if the can be removed.
    */
   spe = (SQL_POOL_ENTRY *)spd->pool_entries->first();
   while (spe) {
      spe_next = (SQL_POOL_ENTRY *)spd->pool_entries->get_next(spe);

      /*
        * See if this is a unreferenced connection.
        * And its been idle for more then idle_timeout seconds.
        */
      if (spe->reference_count == 0 && ((now - spe->last_update) >= spd->idle_timeout)) {
         spd->pool_entries->remove(spe);
         db_close_database(NULL, spe->db_handle);
         free(spe);
         spd->nr_connections--;
         cnt--;
         /*
          * See if we have freed enough.
          */
         if (cnt <= 0) {
            break;
         }
      }

      spe = spe_next;
   }

   /*
    * Now that the pool has shrunk give each entry a new id.
    */
   cnt = 0;
   foreach_dlist(spe, spd->pool_entries) {
      spe->id = cnt++;
   }
}

/*
 * Find the connection pool with the correct connections.
 * This function should be called with the mutex held.
 */
static inline SQL_POOL_DESCRIPTOR *sql_find_pool_descriptor(const char *db_drivername,
                                                            const char *db_name,
                                                            const char *db_address,
                                                            int db_port)
{
   SQL_POOL_DESCRIPTOR *spd;
   SQL_POOL_ENTRY *spe;

   foreach_dlist(spd, db_pooling_descriptors) {
      if (spd->active) {
         foreach_dlist(spe, spd->pool_entries) {
            if (spe->db_handle->db_match_database(db_drivername, db_name,
                                                  db_address, db_port)) {
               return spd;
            }
         }
      }
   }
   return NULL;
}

/*
 * Find a free connection in a certain connection pool.
 * This function should be called with the mutex held.
 */
static inline SQL_POOL_ENTRY *sql_find_free_connection(SQL_POOL_DESCRIPTOR *spd)
{
   SQL_POOL_ENTRY *spe;

   foreach_dlist(spe, spd->pool_entries) {
      if (spe->reference_count == 0) {
         return spe;
      }
   }
   return NULL;
}

/*
 * Find a connection in a certain connection pool.
 * This function should be called with the mutex held.
 */
static inline SQL_POOL_ENTRY *sql_find_first_connection(SQL_POOL_DESCRIPTOR *spd)
{
   SQL_POOL_ENTRY *spe;

   foreach_dlist(spe, spd->pool_entries) {
      return spe;
   }
   return NULL;
}

/*
 * Get a new connection from the pool.
 */
B_DB *db_sql_get_pooled_connection(JCR *jcr,
                                   const char *db_drivername,
                                   const char *db_name,
                                   const char *db_user,
                                   const char *db_password,
                                   const char *db_address,
                                   int db_port,
                                   const char *db_socket,
                                   bool mult_db_connections,
                                   bool disable_batch_insert,
                                   bool need_private,
                                   bool try_reconnect,
                                   bool exit_on_fatal)
{
   int cnt = 0;
   SQL_POOL_DESCRIPTOR *wanted_pool;
   SQL_POOL_ENTRY *use_connection = NULL;
   B_DB *db_handle = NULL;
   time_t now;

   now = time(NULL);
#if defined(HAVE_DYNAMIC_CATS_BACKENDS)
   Dmsg2(100, "db_sql_get_pooled_connection get new connection for connection to database %s, backend type %s\n",
         db_name, db_drivername);
#else
   Dmsg1(100, "db_sql_get_pooled_connection get new connection for connection to database %s\n",
         db_name);
#endif

   /*
    * See if pooling is enabled.
    */
   if (!db_pooling_descriptors) {
      return db_sql_get_non_pooled_connection(jcr,
                                              db_drivername,
                                              db_name,
                                              db_user,
                                              db_password,
                                              db_address,
                                              db_port,
                                              db_socket,
                                              mult_db_connections,
                                              disable_batch_insert,
                                              need_private,
                                              try_reconnect,
                                              exit_on_fatal);
   }

   P(mutex);
   /*
    * Try to lookup the pool.
    */
   wanted_pool = sql_find_pool_descriptor(db_drivername, db_name, db_address, db_port);
   if (wanted_pool) {
      /*
       * Loop while trying to find a connection.
       */
      while (1) {
         /*
          * If we can return a shared connection and when need_private is not
          * explicitly set try to match an existing connection. Otherwise we
          * want a free connection.
          */
         if (!mult_db_connections && !need_private) {
            use_connection = sql_find_first_connection(wanted_pool);
         } else {
            use_connection = sql_find_free_connection(wanted_pool);
         }
         if (use_connection) {
            /*
             * See if the connection match needs validation.
             */
            if ((now - use_connection->last_update) >= wanted_pool->validate_timeout) {
               if (!db_validate_connection(use_connection->db_handle)) {
                  /*
                   * Connection seems to be dead kill it from the pool.
                   */
                  wanted_pool->pool_entries->remove(use_connection);
                  db_close_database(jcr, use_connection->db_handle);
                  free(use_connection);
                  wanted_pool->nr_connections--;
                  continue;
               }
            }
            goto ok_out;
         } else {
            if (mult_db_connections || need_private) {
               /*
                * Cannot find an already open connection that is unused.
                * See if there is still room to grow the pool if not this is it.
                * We just give back a non pooled connection which gets a proper cleanup
                * anyhow when it discarded using db_sql_close_pooled_connection.
                */
               if (wanted_pool->nr_connections >= wanted_pool->max_connections) {
                  db_handle = db_sql_get_non_pooled_connection(jcr,
                                                               db_drivername,
                                                               db_name,
                                                               db_user,
                                                               db_password,
                                                               db_address,
                                                               db_port,
                                                               db_socket,
                                                               mult_db_connections,
                                                               disable_batch_insert,
                                                               need_private,
                                                               try_reconnect,
                                                               exit_on_fatal);
                  goto bail_out;
               }

               Dmsg0(100, "db_sql_get_pooled_connection trying to grow connection pool for getting free connection\n");
               sql_pool_grow(wanted_pool);
            } else {
               /*
                * Request for a shared connection and no connection gets through the validation.
                * e.g. all connections in the pool have failed.
                * This should never happen so lets abort things and let the upper layer handle this.
                */
               goto bail_out;
            }
         }
      }
   } else {
      /*
       * Pooling not enabled for this connection use non pooling.
       */
      db_handle = db_sql_get_non_pooled_connection(jcr,
                                                   db_drivername,
                                                   db_name,
                                                   db_user,
                                                   db_password,
                                                   db_address,
                                                   db_port,
                                                   db_socket,
                                                   mult_db_connections,
                                                   disable_batch_insert,
                                                   need_private,
                                                   try_reconnect,
                                                   exit_on_fatal);
      goto bail_out;
   }

ok_out:
   use_connection->reference_count++;
   use_connection->last_update = now;
   db_handle = use_connection->db_handle;

   /*
    * Set the is_private flag of this database connection to the wanted state.
    */
   db_handle->set_private(need_private);

bail_out:
   V(mutex);
   return db_handle;
}

/*
 * Put a connection back onto the pool for reuse.
 *
 * The abort flag is set when we encounter a dead or misbehaving connection
 * which needs to be closed right away and should not be reused.
 */
void db_sql_close_pooled_connection(JCR *jcr, B_DB *mdb, bool abort)
{
   SQL_POOL_ENTRY *spe, *spe_next;
   SQL_POOL_DESCRIPTOR *spd, *spd_next;
   bool found = false;
   time_t now;

   /*
    * See if pooling is enabled.
    */
   if (!db_pooling_descriptors) {
      db_close_database(jcr, mdb);
      return;
   }

   P(mutex);

   /*
    * See what connection is freed.
    */
   now = time(NULL);

   spd = (SQL_POOL_DESCRIPTOR *)db_pooling_descriptors->first();
   while (spd) {
      spd_next = (SQL_POOL_DESCRIPTOR *)db_pooling_descriptors->get_next(spd);

      if (!spd->pool_entries) {
         spd = spd_next;
         continue;
      }

      spe = (SQL_POOL_ENTRY *)spd->pool_entries->first();
      while (spe) {
         spe_next = (SQL_POOL_ENTRY *)spd->pool_entries->get_next(spe);

         if (spe->db_handle == mdb) {
            found = true;
            if (!abort) {
               /*
                * End any active transactions.
                */
               db_end_transaction(jcr, mdb);

               /*
                * Decrement reference count and update last update field.
                */
               spe->reference_count--;
               time(&spe->last_update);

               Dmsg3(100, "db_sql_close_pooled_connection decrementing reference count of connection %d now %d, backend type %s\n",
                     spe->id, spe->reference_count, spe->db_handle->db_get_type());

               /*
                * Clear the is_private flag if this is a free connection again.
                */
               if (spe->reference_count == 0) {
                  mdb->set_private(false);
               }

               /*
                * See if this is a free on an inactive pool and this was the last reference.
                */
               if (!spd->active && spe->reference_count == 0) {
                  spd->pool_entries->remove(spe);
                  db_close_database(jcr, spe->db_handle);
                  free(spe);
                  spd->nr_connections--;
               }
            } else {
               Dmsg3(100, "db_sql_close_pooled_connection aborting connection to database %s reference count %d, backend type %s\n",
                     spe->db_handle->get_db_name(), spe->reference_count, spe->db_handle->db_get_type());
               spd->pool_entries->remove(spe);
               db_close_database(jcr, spe->db_handle);
               free(spe);
               spd->nr_connections--;
            }

            /*
             * No need to search further if we found the item we were looking for.
             */
            break;
         }

         spe = spe_next;
      }

      /*
       * See if this is an inactive pool and it has no connections on it anymore.
       */
      if (!spd->active && spd->nr_connections == 0) {
         db_pooling_descriptors->remove(spd);
         delete spd->pool_entries;
         free(spd);
      } else {
         /*
          * See if we can shrink the connection pool.
          * Only try to shrink when the last update on the pool was more then the validate time ago.
          */
         if ((now - spd->last_update) >= spd->validate_timeout) {
            Dmsg0(100, "db_sql_close_pooled_connection trying to shrink connection pool\n");
            sql_pool_shrink(spd);
         }
      }

      /*
       * No need to search further if we found the item we were looking for.
       */
      if (found) {
         break;
      }

      spd = spd_next;
   }

   /*
    * If we didn't find this mdb on any pooling chain we are not pooling
    * this connection and we just close the connection.
    */
   if (!found) {
      db_close_database(jcr, mdb);
   }

   V(mutex);
}

#else /* HAVE_SQL_POOLING */

/*
 * Initialize the sql connection pool.
 * For non pooling this is a no-op.
 */
bool db_sql_pool_initialize(const char *db_drivername,
                            const char *db_name,
                            const char *db_user,
                            const char *db_password,
                            const char *db_address,
                            int db_port,
                            const char *db_socket,
                            bool disable_batch_insert,
                            bool try_reconnect,
                            bool exit_on_fatal,
                            int min_connections,
                            int max_connections,
                            int increment_connections,
                            int idle_timeout,
                            int validate_timeout)
{
   return true;
}

/*
 * Cleanup the sql connection pools.
 * For non pooling this is a no-op.
 */
void db_sql_pool_destroy(void)
{
}

/*
 * Flush the sql connection pools.
 * For non pooling this is a no-op.
 */
void db_sql_pool_flush(void)
{
}

/*
 * Get a new connection from the pool.
 * For non pooling we just call db_sql_get_non_pooled_connection.
 */
B_DB *db_sql_get_pooled_connection(JCR *jcr,
                                   const char *db_drivername,
                                   const char *db_name,
                                   const char *db_user,
                                   const char *db_password,
                                   const char *db_address,
                                   int db_port,
                                   const char *db_socket,
                                   bool mult_db_connections,
                                   bool disable_batch_insert,
                                   bool need_private,
                                   bool try_reconnect,
                                   bool exit_on_fatal)
{
   return db_sql_get_non_pooled_connection(jcr,
                                           db_drivername,
                                           db_name,
                                           db_user,
                                           db_password,
                                           db_address,
                                           db_port,
                                           db_socket,
                                           mult_db_connections,
                                           disable_batch_insert,
                                           need_private,
                                           try_reconnect,
                                           exit_on_fatal);
}

/*
 * Put a connection back onto the pool for reuse.
 * For non pooling we just do a db_close_database.
 */
void db_sql_close_pooled_connection(JCR *jcr, B_DB *mdb, bool abort)
{
   db_close_database(jcr, mdb);
}

#endif /* HAVE_SQL_POOLING */
#endif /* HAVE_SQLITE3 || HAVE_MYSQL || HAVE_POSTGRESQL || HAVE_INGRES || HAVE_DBI */
