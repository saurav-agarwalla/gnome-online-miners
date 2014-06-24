/*
 * GNOME Online Miners - crawls through your online content
 * Copyright (c) 2011, 2012, 2013 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * Author: Cosimo Cecchi <cosimoc@redhat.com>
 *
 */

#include "config.h"

#include <gdata/gdata.h>

#include "gom-utils.h"
#include "gom-gdata-miner.h"

#define MINER_IDENTIFIER "gd:gdata:miner:86ec9bc9-c242-427f-aa19-77b5a2c9b6f0"
#define STARRED_CATEGORY_TERM "http://schemas.google.com/g/2005/labels#starred"
#define PARENT_LINK_REL "http://schemas.google.com/docs/2007#parent"

G_DEFINE_TYPE (GomGDataMiner, gom_gdata_miner, GOM_TYPE_MINER)

static gboolean
account_miner_job_process_entry (GomAccountMinerJob *job,
                                 GDataDocumentsService *service,
                                 GDataDocumentsEntry *doc_entry,
                                 GError **error)
{
  GDataEntry *entry = GDATA_ENTRY (doc_entry);
  gchar *resource = NULL;
  gchar *date, *identifier;
  const gchar *class = NULL;
  const gchar *mimetype_override = NULL;
  gboolean mtime_changed, resource_exists;
  gint64 new_mtime;

  GList *authors, *l, *parents = NULL;
  GDataAuthor *author;
  GDataLink *parent;

  GDataLink *alternate;
  const gchar *alternate_uri;

  GList *categories;
  GDataCategory *category;
  gboolean starred = FALSE;

  GDataFeed *access_rules = NULL;

  if (GDATA_IS_DOCUMENTS_FOLDER (doc_entry))
    {
      GDataLink *link;

      link = gdata_entry_look_up_link (entry, GDATA_LINK_SELF);
      identifier = g_strdup_printf ("gd:collection:%s", gdata_link_get_uri (link));
    }
  else
    identifier = g_strdup (gdata_entry_get_id (entry));

  /* remove from the list of the previous resources */
  g_hash_table_remove (job->previous_resources, identifier);

  if (GDATA_IS_DOCUMENTS_PRESENTATION (doc_entry))
    class = "nfo:Presentation";
  else if (GDATA_IS_DOCUMENTS_SPREADSHEET (doc_entry))
    class = "nfo:Spreadsheet";
  else if (GDATA_IS_DOCUMENTS_TEXT (doc_entry))
    class = "nfo:PaginatedTextDocument";
  else if (GDATA_IS_DOCUMENTS_DRAWING (doc_entry))
    class = "nfo:PaginatedTextDocument";
  else if (GDATA_IS_DOCUMENTS_PDF (doc_entry))
    class = "nfo:PaginatedTextDocument";
  else if (GDATA_IS_DOCUMENTS_FOLDER (doc_entry))
    class = "nfo:DataContainer";

  resource = gom_tracker_sparql_connection_ensure_resource
    (job->connection,
     job->cancellable, error,
     &resource_exists,
     job->datasource_urn, identifier,
     "nfo:RemoteDataObject", class, NULL);

  if (*error != NULL)
    goto out;

  gom_tracker_update_datasource (job->connection, job->datasource_urn,
                                 resource_exists, identifier, resource,
                                 job->cancellable, error);

  if (*error != NULL)
    goto out;

  new_mtime = gdata_entry_get_updated (entry);
  mtime_changed = gom_tracker_update_mtime (job->connection, new_mtime,
                                            resource_exists, identifier, resource,
                                            job->cancellable, error);

  if (*error != NULL)
    goto out;

  /* avoid updating the DB if the entry already exists and has not
   * been modified since our last run.
   */
  if (!mtime_changed)
    goto out;

  /* the resource changed - just set all the properties again */
  alternate = gdata_entry_look_up_link (entry, GDATA_LINK_ALTERNATE);
  alternate_uri = gdata_link_get_uri (alternate);

  gom_tracker_sparql_connection_insert_or_replace_triple
    (job->connection,
     job->cancellable, error,
     job->datasource_urn, resource,
     "nie:url", alternate_uri);

  if (*error != NULL)
    goto out;

  /* fake a drawing mimetype, so Documents can get the correct icon */
  if (GDATA_IS_DOCUMENTS_DRAWING (doc_entry))
    mimetype_override = "application/vnd.sun.xml.draw";
  else if (GDATA_IS_DOCUMENTS_PDF (doc_entry))
    mimetype_override = "application/pdf";

  gom_tracker_sparql_connection_insert_or_replace_triple
    (job->connection,
     job->cancellable, error,
     job->datasource_urn, resource,
     "nie:mimeType", mimetype_override);

  if (*error != NULL)
    goto out;

  parents = gdata_entry_look_up_links (entry, PARENT_LINK_REL);
  for (l = parents; l != NULL; l = l->next)
    {
      gchar *parent_resource_urn, *parent_resource_id;

      parent = l->data;
      parent_resource_id =
        g_strdup_printf ("gd:collection:%s", gdata_link_get_uri (parent));

      parent_resource_urn = gom_tracker_sparql_connection_ensure_resource
        (job->connection, job->cancellable, error,
         NULL,
         job->datasource_urn, parent_resource_id,
         "nfo:RemoteDataObject", "nfo:DataContainer", NULL);
      g_free (parent_resource_id);

      if (*error != NULL)
        goto out;

      gom_tracker_sparql_connection_insert_or_replace_triple
        (job->connection,
         job->cancellable, error,
         job->datasource_urn, resource,
         "nie:isPartOf", parent_resource_urn);
      g_free (parent_resource_urn);

      if (*error != NULL)
        goto out;
    }

  categories = gdata_entry_get_categories (entry);
  for (l = categories; l != NULL; l = l->next)
    {
      category = l->data;
      if (g_strcmp0 (gdata_category_get_term (category), STARRED_CATEGORY_TERM) == 0)
        {
          starred = TRUE;
          break;
        }
    }

  gom_tracker_sparql_connection_toggle_favorite
    (job->connection,
     job->cancellable, error,
     resource, starred);

  if (*error != NULL)
    goto out;

  gom_tracker_sparql_connection_insert_or_replace_triple
    (job->connection,
     job->cancellable, error,
     job->datasource_urn, resource,
     "nie:description", gdata_entry_get_summary (entry));

  if (*error != NULL)
    goto out;

  gom_tracker_sparql_connection_insert_or_replace_triple
    (job->connection,
     job->cancellable, error,
     job->datasource_urn, resource,
     "nie:title", gdata_entry_get_title (entry));

  if (*error != NULL)
    goto out;

  authors = gdata_entry_get_authors (entry);
  for (l = authors; l != NULL; l = l->next)
    {
      gchar *contact_resource;

      author = l->data;

      contact_resource = gom_tracker_utils_ensure_contact_resource (job->connection,
                                                                    job->cancellable, error,
                                                                    gdata_author_get_email_address (author),
                                                                    gdata_author_get_name (author));

      if (*error != NULL)
        goto out;

      gom_tracker_sparql_connection_insert_or_replace_triple
        (job->connection,
         job->cancellable, error,
         job->datasource_urn, resource,
         "nco:creator", contact_resource);

      if (*error != NULL)
        goto out;

      g_free (contact_resource);
    }

  access_rules = gdata_access_handler_get_rules (GDATA_ACCESS_HANDLER (entry),
                                                 GDATA_SERVICE (service),
                                                 job->cancellable,
                                                 NULL, NULL, error);

  if (*error != NULL)
      goto out;

  for (l = gdata_feed_get_entries (access_rules); l != NULL; l = l->next)
    {
      GDataAccessRule *rule = l->data;
      const gchar *scope_type;
      const gchar *scope_value;
      gchar *contact_resource;

      gdata_access_rule_get_scope (rule, &scope_type, &scope_value);

      /* default scope access means the document is completely public */
      if (g_strcmp0 (scope_type, GDATA_ACCESS_SCOPE_DEFAULT) == 0)
        continue;

      /* skip domain scopes */
      if (g_strcmp0 (scope_type, GDATA_ACCESS_SCOPE_DOMAIN) == 0)
        continue;

      contact_resource = gom_tracker_utils_ensure_contact_resource (job->connection,
                                                                    job->cancellable, error,
                                                                    scope_value,
                                                                    "");

      gom_tracker_sparql_connection_insert_or_replace_triple
        (job->connection,
         job->cancellable, error,
         job->datasource_urn, resource,
         "nco:contributor", contact_resource);

      g_free (contact_resource);

      if (*error != NULL)
        goto out;
    }

  date = gom_iso8601_from_timestamp (gdata_entry_get_published (entry));
  gom_tracker_sparql_connection_insert_or_replace_triple
    (job->connection,
     job->cancellable, error,
     job->datasource_urn, resource,
     "nie:contentCreated", date);
  g_free (date);

  if (*error != NULL)
    goto out;

 out:
  g_clear_object (&access_rules);
  g_free (resource);
  g_free (identifier);

  g_list_free (parents);

  if (*error != NULL)
    return FALSE;

  return TRUE;
}

static gboolean
account_miner_job_process_photo (GomAccountMinerJob *job,
                                 GDataPicasaWebFile *photo,
                                 const gchar *parent_resource_urn,
                                 GError **error)
{
  GDataLink *link;
  GList *media_content;
  const gchar *photo_id;
  const gchar *photo_uri;
  const gchar *photo_title;
  const gchar *photo_summary;
  const gchar *mime;
  gint64 photo_timestamp;
  gint64 photo_updated;
  gchar *photo_timestamp_iso8601;

  gchar *photo_exposure;
  gboolean photo_flash;
  gchar *photo_focal_length;
  gchar *photo_fstop;
  gchar *photo_iso;
  const gchar *photo_make;
  const gchar *photo_model;
  gchar *photo_width;
  gchar *photo_height;
  const gchar *photo_credit;
  const gchar *flash_off = "http://www.tracker-project.org/temp/nmm#flash-off";
  const gchar *flash_on = "http://www.tracker-project.org/temp/nmm#flash-on";

  gchar *identifier;
  const gchar *class = "nmm:Photo";
  gchar *resource = NULL;
  gboolean resource_exists, mtime_changed;
  gchar *contact_resource;
  gchar *equipment_resource;

  photo_id = gdata_entry_get_id (GDATA_ENTRY (photo));
  identifier = g_strdup_printf ("google:%s", photo_id);

  /* remove from the list of the previous resources */
  g_hash_table_remove (job->previous_resources, identifier);

  resource = gom_tracker_sparql_connection_ensure_resource
    (job->connection,
     job->cancellable, error,
     &resource_exists,
     job->datasource_urn, identifier,
     "nfo:RemoteDataObject", class, NULL);

  if (*error != NULL)
    goto out;

  gom_tracker_update_datasource (job->connection, job->datasource_urn,
                                 resource_exists, identifier, resource,
                                 job->cancellable, error);
  if (*error != NULL)
    goto out;

  /* Check updated time to avoid updating the DB if it has not
   * been modified since our last run
   */
  photo_updated = gdata_entry_get_updated (GDATA_ENTRY (photo));
  mtime_changed = gom_tracker_update_mtime (job->connection, photo_updated,
                                            resource_exists, identifier, resource,
                                            job->cancellable, error);

  if (*error != NULL)
    goto out;

  /* avoid updating the DB if the resource already exists and has not
   * been modified since our last run.
   */

  if (!mtime_changed)
    goto out;

  /* the resource changed - just set all the properties again */
  link = gdata_entry_look_up_link (GDATA_ENTRY (photo), GDATA_LINK_ALTERNATE);
  photo_uri = gdata_link_get_uri (link);
  gom_tracker_sparql_connection_insert_or_replace_triple
    (job->connection,
     job->cancellable, error,
     job->datasource_urn, resource,
     "nie:url", photo_uri);

  if (*error != NULL)
    goto out;

  photo_summary = gdata_entry_get_summary ((GDATA_ENTRY (photo)));
  gom_tracker_sparql_connection_insert_or_replace_triple
    (job->connection,
     job->cancellable, error,
     job->datasource_urn, resource,
     "nie:description", photo_summary);

  if (*error != NULL)
    goto out;

  gom_tracker_sparql_connection_insert_or_replace_triple
    (job->connection,
     job->cancellable, error,
     job->datasource_urn, resource,
     "nie:isPartOf", parent_resource_urn);

  if (*error != NULL)
    goto out;

  media_content = gdata_picasaweb_file_get_contents (photo);
  mime = gdata_media_content_get_content_type (GDATA_MEDIA_CONTENT (media_content->data));
  gom_tracker_sparql_connection_insert_or_replace_triple
    (job->connection,
     job->cancellable, error,
     job->datasource_urn, resource,
     "nie:mimeType", mime);

  if (*error != NULL)
    goto out;

  photo_title = gdata_entry_get_title ((GDATA_ENTRY (photo)));
  gom_tracker_sparql_connection_insert_or_replace_triple
    (job->connection,
     job->cancellable, error,
     job->datasource_urn, resource,
     "nie:title", photo_title);

  if (*error != NULL)
    goto out;

  photo_credit = gdata_picasaweb_file_get_credit (photo);
  contact_resource = gom_tracker_utils_ensure_contact_resource
    (job->connection,
     job->cancellable, error,
     job->datasource_urn, photo_credit);

  if (*error != NULL)
    goto out;

  gom_tracker_sparql_connection_insert_or_replace_triple
    (job->connection,
     job->cancellable, error,
     job->datasource_urn, resource,
     "nco:creator", contact_resource);

  g_free (contact_resource);
  if (*error != NULL)
    goto out;

  photo_exposure = g_strdup_printf ("%f", gdata_picasaweb_file_get_exposure (photo));
  gom_tracker_sparql_connection_insert_or_replace_triple
    (job->connection,
     job->cancellable, error,
     job->datasource_urn, resource,
     "nmm:exposureTime", photo_exposure);

  if (*error != NULL)
    goto out;

  photo_focal_length = g_strdup_printf ("%f", gdata_picasaweb_file_get_focal_length (photo));
  gom_tracker_sparql_connection_insert_or_replace_triple
    (job->connection,
     job->cancellable, error,
     job->datasource_urn, resource,
     "nmm:focalLength", photo_focal_length);

  if (*error != NULL)
    goto out;

  photo_fstop = g_strdup_printf ("%f", gdata_picasaweb_file_get_fstop (photo));
  gom_tracker_sparql_connection_insert_or_replace_triple
    (job->connection,
     job->cancellable, error,
     job->datasource_urn, resource,
     "nmm:fnumber", photo_fstop);

  if (*error != NULL)
    goto out;

  photo_iso = g_strdup_printf ("%ld", gdata_picasaweb_file_get_iso (photo));
  gom_tracker_sparql_connection_insert_or_replace_triple
    (job->connection,
     job->cancellable, error,
     job->datasource_urn, resource,
     "nmm:isoSpeed", photo_iso);

  if (*error != NULL)
    goto out;

  photo_flash = gdata_picasaweb_file_get_flash (photo);
  gom_tracker_sparql_connection_insert_or_replace_triple
    (job->connection,
     job->cancellable, error,
     job->datasource_urn, resource,
     "nmm:flash", photo_flash ? flash_on : flash_off);

  if (*error != NULL)
    goto out;

  if (photo_make != NULL || photo_model != NULL)
    {
      equipment_resource = gom_tracker_sparql_connection_ensure_resource
        (job->connection,
         job->cancellable, error,
         &resource_exists,
         job->datasource_urn, resource,
         "nfo:Equipment", NULL);

      if (*error != NULL)
        goto out;

      photo_make = gdata_picasaweb_file_get_make (photo);
      gom_tracker_sparql_connection_insert_or_replace_triple
        (job->connection,
         job->cancellable, error,
         job->datasource_urn, equipment_resource,
         "nfo:manufacturer", photo_make);

      if (*error != NULL)
        goto out;

      photo_model = gdata_picasaweb_file_get_model (photo);
      gom_tracker_sparql_connection_insert_or_replace_triple
        (job->connection,
         job->cancellable, error,
         job->datasource_urn, equipment_resource,
         "nfo:model", photo_model);

      if (*error != NULL)
        goto out;

      gom_tracker_sparql_connection_insert_or_replace_triple
        (job->connection,
         job->cancellable, error,
         job->datasource_urn, resource,
         "nfo:equipment", equipment_resource);

      if (*error != NULL)
        goto out;
    }

  photo_width = g_strdup_printf ("%d", gdata_picasaweb_file_get_width (photo));
  gom_tracker_sparql_connection_insert_or_replace_triple
    (job->connection,
     job->cancellable, error,
     job->datasource_urn, resource,
     "nfo:width", photo_width);

  if (*error != NULL)
    goto out;

  photo_height = g_strdup_printf ("%d", gdata_picasaweb_file_get_height (photo));
  gom_tracker_sparql_connection_insert_or_replace_triple
    (job->connection,
     job->cancellable, error,
     job->datasource_urn, resource,
     "nfo:height", photo_height);

  if (*error != NULL)
    goto out;

  photo_timestamp = gdata_picasaweb_file_get_timestamp (photo);
  /* Timestamp is in milliseconds. Divide by 1000 to convert it to seconds. */
  photo_timestamp_iso8601 = gom_iso8601_from_timestamp (photo_timestamp / 1000);
  gom_tracker_sparql_connection_insert_or_replace_triple
    (job->connection,
     job->cancellable, error,
     job->datasource_urn, resource,
     "nie:contentCreated", photo_timestamp_iso8601);

  if (*error != NULL)
    goto out;

  out:
  g_free (resource);
  g_free (identifier);
  g_free (photo_exposure);
  g_free (photo_focal_length);
  g_free (photo_fstop);
  g_free (photo_iso);
  g_free (photo_timestamp_iso8601);
  g_free (equipment_resource);
  g_free (photo_width);
  g_free (photo_height);

  if (*error != NULL)
    return FALSE;

  return TRUE;
}

static gboolean
account_miner_job_process_album (GomAccountMinerJob *job,
                                 GDataPicasaWebService *service,
                                 GDataPicasaWebAlbum *album,
                                 GError **error)
{
  GDataFeed *feed;
  GDataLink *link;
  GDataPicasaWebQuery *query;
  GDataAuthor *author;
  const gchar *album_id;
  const gchar *album_title;
  const gchar *album_summary;
  const gchar *album_uri;
  const gchar *album_user;
  gint64 album_timestamp;
  gint64 album_updated;
  gchar *album_timestamp_iso8601;

  gchar *identifier;
  const gchar *class = "nfo:DataContainer";
  gchar *resource = NULL;
  gboolean resource_exists, mtime_changed;
  GList *l, *authors, *photos = NULL;

  album_id = gdata_entry_get_id (GDATA_ENTRY (album));
  identifier = g_strdup_printf ("photos:collection:google:%s", album_id);

  /* remove from the list of the previous resources */
  g_hash_table_remove (job->previous_resources, identifier);

  resource = gom_tracker_sparql_connection_ensure_resource
    (job->connection,
     job->cancellable, error,
     &resource_exists,
     job->datasource_urn, identifier,
     "nfo:RemoteDataObject", class,
     NULL);

  if (*error != NULL)
    goto out;

  gom_tracker_update_datasource
    (job->connection, job->datasource_urn,
     resource_exists, identifier, resource,
     job->cancellable, error);

  if (*error != NULL)
    goto out;

  /* Check updated time to avoid updating the DB if it has not
   * been modified since our last run
   */
  album_updated = gdata_entry_get_updated (GDATA_ENTRY (album));
  mtime_changed = gom_tracker_update_mtime (job->connection, album_updated,
                                            resource_exists, identifier, resource,
                                            job->cancellable, error);

  if (*error != NULL)
    goto out;

  /* avoid updating the DB if the resource already exists and has not
   * been modified since our last run.
   */

  if (!mtime_changed)
    goto album_photos;

  /* the resource changed - just set all the properties again */
  link = gdata_entry_look_up_link (GDATA_ENTRY (album), GDATA_LINK_ALTERNATE);
  album_uri = gdata_link_get_uri (link);
  gom_tracker_sparql_connection_insert_or_replace_triple
    (job->connection,
     job->cancellable, error,
     job->datasource_urn, resource,
     "nie:url", album_uri);

  if (*error != NULL)
    goto out;

  album_summary = gdata_entry_get_summary ((GDATA_ENTRY (album)));
  gom_tracker_sparql_connection_insert_or_replace_triple
    (job->connection,
     job->cancellable, error,
     job->datasource_urn, resource,
     "nie:description", album_summary);

  if (*error != NULL)
    goto out;

  album_title = gdata_entry_get_title ((GDATA_ENTRY (album)));
  gom_tracker_sparql_connection_insert_or_replace_triple
    (job->connection,
     job->cancellable, error,
     job->datasource_urn, resource,
     "nie:title", album_title);

  if (*error != NULL)
    goto out;

  authors = gdata_entry_get_authors (GDATA_ENTRY (album));
  for (l = authors; l != NULL; l = l->next)
    {
      gchar *contact_resource;

      author = l->data;

      contact_resource = gom_tracker_utils_ensure_contact_resource (job->connection,
                                                                    job->cancellable, error,
                                                                    gdata_author_get_email_address (author),
                                                                    gdata_author_get_name (author));

      if (*error != NULL)
        goto out;

      gom_tracker_sparql_connection_insert_or_replace_triple
        (job->connection,
         job->cancellable, error,
         job->datasource_urn, resource,
         "nco:creator", contact_resource);

      if (*error != NULL)
        goto out;

      g_free (contact_resource);
    }

  album_timestamp = gdata_picasaweb_album_get_timestamp (album);
  /* Timestamp is in milliseconds. Divide by 1000 to convert it to seconds. */
  album_timestamp_iso8601 = gom_iso8601_from_timestamp (album_timestamp / 1000);
  gom_tracker_sparql_connection_insert_or_replace_triple
    (job->connection,
     job->cancellable, error,
     job->datasource_urn, resource,
     "nie:contentCreated", album_timestamp_iso8601);

  if (*error != NULL)
    goto out;

  /* Album photos */
  album_photos:
  query = gdata_picasaweb_query_new (NULL);
  gdata_picasaweb_query_set_image_size (query, "d");
  feed = gdata_picasaweb_service_query_files (service, album, GDATA_QUERY (query),
                                              job->cancellable, NULL, NULL, error);

  g_object_unref (query);

  if (feed == NULL)
    goto out;

  photos = gdata_feed_get_entries (GDATA_FEED (feed));

  for (l = photos; l != NULL; l = l->next)
    {
      account_miner_job_process_photo (job, GDATA_PICASAWEB_FILE (l->data), resource, error);

      if (*error != NULL)
        {
          const gchar *photo_id;

          photo_id = gdata_picasaweb_file_get_id (GDATA_PICASAWEB_FILE (l->data));
          g_warning ("Unable to process photo %s: %s", photo_id, (*error)->message);
          g_clear_error (error);
        }
    }

  out:
  g_free (resource);
  g_free (identifier);
  g_free (album_timestamp_iso8601);
  g_object_unref (feed);

  if (*error != NULL)
    return FALSE;

  return TRUE;
}

static void
query_gdata_documents (GomAccountMinerJob *job,
                       GDataDocumentsService *service,
                       GError **error)
{
  GDataDocumentsQuery *query;
  GDataDocumentsFeed *feed;
  GList *entries, *l;

  if (service == NULL)
    {
      /* FIXME: use proper #defines and enumerated types */
      g_set_error (error,
                   g_quark_from_static_string ("gom-error"),
                   0,
                   "Can not query without a service");
      return;
    }

  query = gdata_documents_query_new (NULL);
  gdata_documents_query_set_show_folders (query, TRUE);
  feed = gdata_documents_service_query_documents
    (service, query,
     job->cancellable, NULL, NULL, error);

  g_object_unref (query);

  if (feed == NULL)
    return;

  entries = gdata_feed_get_entries (GDATA_FEED (feed));
  for (l = entries; l != NULL; l = l->next)
    {
      account_miner_job_process_entry (job, service, l->data, error);

      if (*error != NULL)
        {
          g_warning ("Unable to process entry %p: %s", l->data, (*error)->message);
          g_clear_error (error);
        }
    }

  g_object_unref (feed);
}

static void
query_gdata_photos (GomAccountMinerJob *job,
                    GDataPicasaWebService *service,
                    GError **error)
{
  GDataFeed *feed;
  GList *albums, *l;

  if (service == NULL)
    {
      /* FIXME: use proper #defines and enumerated types */
      g_set_error (error,
                   g_quark_from_static_string ("gom-error"),
                   0,
                   "Can not query without a service");
      return;
    }

  feed = gdata_picasaweb_service_query_all_albums (service, NULL, NULL, job->cancellable, NULL, NULL, error);

  if (feed == NULL)
    return;

  albums = gdata_feed_get_entries (feed);
  for (l = albums; l != NULL; l = l->next)
    {
      account_miner_job_process_album (job, service, GDATA_PICASAWEB_ALBUM (l->data), error);

      if (*error != NULL)
        {
          const gchar *album_id;

          album_id = gdata_picasaweb_album_get_id (GDATA_PICASAWEB_ALBUM (l->data));
          g_warning ("Unable to process album %s: %s", album_id, (*error)->message);
          g_clear_error (error);
        }
    }

  g_object_unref (feed);
}

static void
query_gdata (GomAccountMinerJob *job,
             GError **error)
{
  gpointer service;

  if (g_hash_table_lookup_extended (job->services, "photos", NULL, &service))
    query_gdata_photos (job, GDATA_PICASAWEB_SERVICE (service), error);

  if (g_hash_table_lookup_extended (job->services, "documents", NULL, &service))
    query_gdata_documents (job, GDATA_DOCUMENTS_SERVICE (service), error);
}

static GHashTable *
create_services (GomMiner *self,
                 GoaObject *object)
{
  GDataGoaAuthorizer *authorizer;
  GDataService *service;
  GHashTable *services;

  services = g_hash_table_new_full (g_str_hash, g_str_equal,
                                    NULL, (GDestroyNotify) g_object_unref);

  authorizer = gdata_goa_authorizer_new (object);

  if (gom_miner_supports_type (self, "documents") && goa_object_peek_documents (object) != NULL)
    {
      service = GDATA_SERVICE (gdata_documents_service_new (GDATA_AUTHORIZER (authorizer)));
      g_hash_table_insert (services, "documents", service);
    }
  if (gom_miner_supports_type (self, "photos") && goa_object_peek_photos (object) != NULL)
    {
      service = GDATA_SERVICE (gdata_picasaweb_service_new (GDATA_AUTHORIZER (authorizer)));
      g_hash_table_insert (services, "photos", service);
    }

  /* the service takes ownership of the authorizer */
  g_object_unref (authorizer);
  return services;
}

static void
gom_gdata_miner_init (GomGDataMiner *miner)
{
}

static void
gom_gdata_miner_class_init (GomGDataMinerClass *klass)
{
  GomMinerClass *miner_class = GOM_MINER_CLASS (klass);

  miner_class->goa_provider_type = "google";
  miner_class->miner_identifier = MINER_IDENTIFIER;
  miner_class->version = 3;

  miner_class->create_services = create_services;
  miner_class->query = query_gdata;
}
